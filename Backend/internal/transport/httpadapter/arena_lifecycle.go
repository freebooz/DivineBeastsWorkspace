// 本文件为竞技状态/迁出/撤销提供内部Bearer入口；公开客户端不能查任意比赛或撤销他人票据。
package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"net/http"
	"strings"
)

// registerArenaLifecycle所有动作调用同一控制服务，真实人数/Boot门禁不能由UI关闭或本地超时绕过。
func registerArenaLifecycle(mux *http.ServeMux, service *gameservercontrol.Service, bearer string) {
	mux.Handle("POST /internal/v1/arena/presence", requireGameServerControlBearer(bearer, func(w http.ResponseWriter, r *http.Request) {
		var input struct {
			PlayerID  string `json:"playerId"`
			SessionID string `json:"sessionId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &input, []string{"playerId", "sessionId"}, []string{"playerId", "sessionId"}); err != nil {
			writeArenaLifecycleError(w, err)
			return
		}
		value, err := service.LookupPlayerPresence(r.Context(), input.PlayerID, input.SessionID)
		if err != nil {
			writeArenaLifecycleError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, value)
	}))
	for _, operation := range []string{"status", "cancel", "finalize"} {
		operation := operation
		mux.Handle("POST /internal/v1/arena/"+operation, requireGameServerControlBearer(bearer, func(w http.ResponseWriter, r *http.Request) {
			var input struct {
				MatchID string `json:"matchId"`
			}
			if err := gateway.DecodeOnlineJSON(r, &input, []string{"matchId"}, []string{"matchId"}); err != nil {
				writeArenaLifecycleError(w, err)
				return
			}
			if operation == "status" {
				value, err := service.GetArenaStatus(r.Context(), input.MatchID)
				if err != nil {
					writeArenaLifecycleError(w, err)
					return
				}
				writeJSON(w, http.StatusOK, value)
				return
			}
			var err error
			if operation == "cancel" {
				err = service.CancelArena(r.Context(), input.MatchID)
			} else {
				err = service.FinalizeArena(r.Context(), input.MatchID)
			}
			if err != nil {
				writeArenaLifecycleError(w, err)
				return
			}
			writeJSON(w, http.StatusOK, map[string]bool{"accepted": true})
		}))
	}
	mux.Handle("POST /internal/v1/arena/revoke-transfer", requireGameServerControlBearer(bearer, func(w http.ResponseWriter, r *http.Request) {
		var input struct {
			TicketID string `json:"ticketId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &input, []string{"ticketId"}, []string{"ticketId"}); err != nil {
			writeArenaLifecycleError(w, err)
			return
		}
		if err := service.RevokeTransfer(r.Context(), input.TicketID); err != nil {
			writeArenaLifecycleError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]bool{"accepted": true})
	}))
}

// writeArenaLifecycleError只输出白名单内部码，不输出端点、认证信息、数据库正文或签票秘密。
func writeArenaLifecycleError(w http.ResponseWriter, err error) {
	code := strings.SplitN(err.Error(), ":", 2)[0]
	status, safe := gateway.OnlineErrorStatus(gateway.ServiceError(code))
	writeError(w, status, safe, nil)
}

// QueryArenaStatus读取真实分配/心跳/位置供Match恢复，不广播到UI。
func (c *GameServerControlClient) QueryArenaStatus(ctx context.Context, matchID string) (gameservercontrol.ArenaStatus, error) {
	var value gameservercontrol.ArenaStatus
	err := c.postAuthorized(ctx, "/internal/v1/arena/status", map[string]string{"matchId": matchID}, &value)
	return value, err
}

// CancelArena停止本比赛新准入并排空，不强行回收仍有玩家的实例。
func (c *GameServerControlClient) CancelArena(ctx context.Context, matchID string) error {
	var response struct {
		Accepted bool `json:"accepted"`
	}
	return c.postAuthorized(ctx, "/internal/v1/arena/cancel", map[string]string{"matchId": matchID}, &response)
}

// FinalizeArena由控制面再次核验人数/Boot，不把本地页面关闭当作已迁出。
func (c *GameServerControlClient) FinalizeArena(ctx context.Context, matchID string) error {
	var response struct {
		Accepted bool `json:"accepted"`
	}
	return c.postAuthorized(ctx, "/internal/v1/arena/finalize", map[string]string{"matchId": matchID}, &response)
}

// RevokeTransfer补偿包括签票响应未知的稳定TicketID，不对公网开放。
func (c *GameServerControlClient) RevokeTransfer(ctx context.Context, ticketID string) error {
	var response struct {
		Accepted bool `json:"accepted"`
	}
	return c.postAuthorized(ctx, "/internal/v1/arena/revoke-transfer", map[string]string{"ticketId": ticketID}, &response)
}

// LookupMatchmakingPresence只查询认证会话本人；返回体验类别供重新分配，绝不使用旧地址作为返回证明。
func (c *GameServerControlClient) LookupMatchmakingPresence(ctx context.Context, player, session string) (gateway.MatchmakingWorldPresence, error) {
	var value gameservercontrol.PlayerPresence
	err := c.postAuthorized(ctx, "/internal/v1/arena/presence", map[string]string{"playerId": player, "sessionId": session}, &value)
	if err != nil {
		return gateway.MatchmakingWorldPresence{}, err
	}
	if value.PlayerID != player || value.SessionID != session || value.GameServerID == "" || value.ExperienceID == "" {
		return gateway.MatchmakingWorldPresence{}, gateway.ServiceError("MATCH_WORLD_REQUIRED")
	}
	return gateway.MatchmakingWorldPresence{GameServerID: value.GameServerID, ExperienceID: value.ExperienceID}, nil
}

var _ gateway.MatchmakingPresencePort = (*GameServerControlClient)(nil)
