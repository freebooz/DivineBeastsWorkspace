// 本文件实现Gateway与Match内部HTTP组队交互；公开JSON契约在Shared，领域对象必须经授权投影后返回。
// 与既有内部部署一致只面向可信服务网络；公网只注册Gateway认证路由，不向客户端暴露本处理器。
package httpadapter

import (
	"context"
	"net/http"

	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/party"
)

// registerPartyInteractions（内部组队路由）严格解析已认证调用方传入的PlayerID；不注册到公网主Gateway。
func registerPartyInteractions(mux *http.ServeMux, service *matchapi.Service) {
	mux.HandleFunc("POST /internal/v1/match/party/state", func(w http.ResponseWriter, r *http.Request) {
		var request struct {
			PlayerID string `json:"playerId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &request, []string{"playerId"}, []string{"playerId"}); err != nil {
			writePartyInteractionError(w, err)
			return
		}
		view, err := service.ReadPartyState(r.Context(), request.PlayerID)
		if err != nil {
			writePartyInteractionError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, view)
	})
	mux.HandleFunc("POST /internal/v1/match/party/invitations", func(w http.ResponseWriter, r *http.Request) {
		var request struct {
			PlayerID string `json:"playerId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &request, []string{"playerId"}, []string{"playerId"}); err != nil {
			writePartyInteractionError(w, err)
			return
		}
		items, err := service.GetPartyInvitations(r.Context(), request.PlayerID)
		if err != nil {
			writePartyInteractionError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, items)
	})
	mux.HandleFunc("POST /internal/v1/match/party/commands", func(w http.ResponseWriter, r *http.Request) {
		// 内部包围体与命令平铺，共用同一严格解码器，不允许嵌套JSON逃过重复键检查。
		var request struct {
			PlayerID string `json:"playerId"`
			party.Command
		}
		if err := gateway.DecodeOnlineJSON(r, &request, []string{"playerId", "partyId", "action", "targetPlayerId", "membershipRequestId", "clientRequestId", "expectedRevision", "ready"}, []string{"playerId", "partyId", "action", "clientRequestId", "expectedRevision"}); err != nil {
			writePartyInteractionError(w, err)
			return
		}
		view, err := service.CommandParty(r.Context(), request.PlayerID, request.Command)
		if err != nil {
			writePartyInteractionError(w, err)
			return
		}
		writeJSON(w, http.StatusOK, view)
	})
}

// writePartyInteractionError（内部安全错误）只返回白名单稳定码，不记录请求正文或数据库细节。
func writePartyInteractionError(w http.ResponseWriter, err error) {
	code := gateway.PartyErrorCode(err)
	if typed, ok := err.(gateway.ServiceError); ok {
		code = string(typed)
	}
	status, code := gateway.OnlineErrorStatus(gateway.ServiceError(code))
	writeError(w, status, code, nil)
}

// ReadPartyState（HTTP本人快照）复用既有内部客户端超时、响应上限与重定向保护。
func (c *MatchClient) ReadPartyState(ctx context.Context, playerID string) (party.Snapshot, error) {
	var value party.Snapshot
	err := c.post(ctx, "/internal/v1/match/party/state", map[string]string{"playerId": playerID}, &value)
	return value, err
}

// CommandParty（HTTP提交命令）Actor单独由Gateway认证上下文传入，不接受客户端覆盖。
func (c *MatchClient) CommandParty(ctx context.Context, playerID string, command party.Command) (party.CommandResult, error) {
	request := struct {
		PlayerID string `json:"playerId"`
		party.Command
	}{PlayerID: playerID, Command: command}
	var value party.CommandResult
	err := c.post(ctx, "/internal/v1/match/party/commands", request, &value)
	return value, err
}

// GetPartyInvitations（HTTP本人收件箱）下游必须返回授权投影而不是完整队伍。
func (c *MatchClient) GetPartyInvitations(ctx context.Context, playerID string) ([]matchapi.PartyInvitationView, error) {
	value := []matchapi.PartyInvitationView{}
	err := c.post(ctx, "/internal/v1/match/party/invitations", map[string]string{"playerId": playerID}, &value)
	return value, err
}

var _ gateway.PartyInteractionPort = (*MatchClient)(nil)
