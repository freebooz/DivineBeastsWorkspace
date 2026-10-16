// 本文件是既有Match服务HTTP生命周期适配；内部Envelope各JSON字符串仍执行完整严格解码，不建立另一套业务规则。
package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/matchmaking"
	"encoding/json"
	"net/http"
	"time"
)

// registerMatchmakingLifecycle只注册可信内部路由，公开客户端仅连接认证Gateway。
func registerMatchmakingLifecycle(mux *http.ServeMux, service *matchapi.Service) {
	mux.HandleFunc("POST /internal/v1/match/matchmaking/state", func(w http.ResponseWriter, r *http.Request) {
		var input struct {
			PlayerID string `json:"playerId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &input, []string{"playerId"}, []string{"playerId"}); err != nil {
			writeMatchLifecycleError(w, err)
			return
		}
		result, err := service.ReadMatchmakingState(r.Context(), input.PlayerID)
		if err != nil {
			writeMatchLifecycleError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	})
	mux.HandleFunc("POST /internal/v1/match/matchmaking/observe", func(w http.ResponseWriter, r *http.Request) {
		var input struct {
			PlayerID         string `json:"playerId"`
			Cursor           string `json:"cursor"`
			WaitMilliseconds int64  `json:"waitMilliseconds"`
		}
		if err := gateway.DecodeOnlineJSON(r, &input, []string{"playerId", "cursor", "waitMilliseconds"}, []string{"playerId", "cursor", "waitMilliseconds"}); err != nil {
			writeMatchLifecycleError(w, err)
			return
		}
		if input.WaitMilliseconds < 0 || input.WaitMilliseconds > 20000 {
			writeMatchLifecycleError(w, gateway.ServiceError("INVALID_REQUEST"))
			return
		}
		result, err := service.ObserveMatchmaking(r.Context(), input.PlayerID, input.Cursor, time.Duration(input.WaitMilliseconds)*time.Millisecond)
		if err != nil {
			writeMatchLifecycleError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	})
	for _, operation := range []string{"commands", "migration"} {
		operation := operation
		mux.HandleFunc("POST /internal/v1/match/matchmaking/"+operation, func(w http.ResponseWriter, r *http.Request) {
			var input struct {
				ActorJSON   string `json:"actorJson"`
				CommandJSON string `json:"commandJson"`
			}
			if err := gateway.DecodeOnlineJSON(r, &input, []string{"actorJson", "commandJson"}, []string{"actorJson", "commandJson"}); err != nil {
				writeMatchLifecycleError(w, err)
				return
			}
			actor, err := gateway.DecodeMatchmakingActorJSON(input.ActorJSON)
			if err != nil {
				writeMatchLifecycleError(w, err)
				return
			}
			if operation == "commands" {
				command, err := gateway.DecodeMatchmakingCommandJSON(input.CommandJSON)
				if err != nil {
					writeMatchLifecycleError(w, err)
					return
				}
				result, err := service.CommandMatchmaking(r.Context(), actor, command)
				if err != nil {
					writeMatchLifecycleError(w, err)
					return
				}
				writeJSON(w, http.StatusOK, result)
			} else {
				command, err := gateway.DecodeMatchmakingMigrationJSON(input.CommandJSON)
				if err != nil {
					writeMatchLifecycleError(w, err)
					return
				}
				result, err := service.GetMatchmakingMigration(r.Context(), actor, command)
				if err != nil {
					writeMatchLifecycleError(w, err)
					return
				}
				writeJSON(w, http.StatusOK, result)
			}
		})
	}
}

// writeMatchLifecycleError只传稳定安全码，不输出数据库/控制面原始异常。
func writeMatchLifecycleError(w http.ResponseWriter, err error) {
	status, code := gateway.OnlineErrorStatus(gateway.ServiceError(gateway.MatchmakingErrorCode(err)))
	writeError(w, status, code, nil)
}

// ReadMatchmakingState读取本人投影，不接受票据枚举。
func (c *MatchClient) ReadMatchmakingState(ctx context.Context, actor string) (matchmaking.PlayerSnapshot, error) {
	var result matchmaking.PlayerSnapshot
	err := c.post(ctx, "/internal/v1/match/matchmaking/state", map[string]string{"playerId": actor}, &result)
	return result, err
}

// ObserveMatchmaking仅只读长等待延长预算，普通写命令保持原有短超时。
func (c *MatchClient) ObserveMatchmaking(ctx context.Context, actor, cursor string, wait time.Duration) (matchmaking.Observation, error) {
	client := *c.client
	client.Timeout = 25 * time.Second
	transport := internalClient{baseURL: c.baseURL, client: &client, bearerToken: c.bearerToken}
	var result matchmaking.Observation
	err := transport.post(ctx, "/internal/v1/match/matchmaking/observe", map[string]any{"playerId": actor, "cursor": cursor, "waitMilliseconds": wait.Milliseconds()}, &result)
	return result, err
}

// matchLifecycleEnvelope只编码可信内部会话/角色，不包含访问令牌或密码。
func matchLifecycleEnvelope(actor matchmaking.ActorContext, command any) (map[string]string, error) {
	actorBytes, err := json.Marshal(actor)
	if err != nil {
		return nil, err
	}
	commandBytes, err := json.Marshal(command)
	if err != nil {
		return nil, err
	}
	return map[string]string{"actorJson": string(actorBytes), "commandJson": string(commandBytes)}, nil
}

// CommandMatchmaking提交真实意图，不将超时解释为远端取消成功。
func (c *MatchClient) CommandMatchmaking(ctx context.Context, actor matchmaking.ActorContext, command matchmaking.Command) (matchmaking.PlayerSnapshot, error) {
	var result matchmaking.PlayerSnapshot
	input, err := matchLifecycleEnvelope(actor, command)
	if err != nil {
		return result, err
	}
	err = c.post(ctx, "/internal/v1/match/matchmaking/commands", input, &result)
	return result, err
}

// GetMatchmakingMigration仅向本人返回秘密准入，调用方必须直接交给既有Session流程。
func (c *MatchClient) GetMatchmakingMigration(ctx context.Context, actor matchmaking.ActorContext, command matchmaking.MigrationRequest) (matchmaking.Migration, error) {
	var result matchmaking.Migration
	input, err := matchLifecycleEnvelope(actor, command)
	if err != nil {
		return result, err
	}
	err = c.post(ctx, "/internal/v1/match/matchmaking/migration", input, &result)
	return result, err
}

var _ gateway.MatchmakingLifecyclePort = (*MatchClient)(nil)
