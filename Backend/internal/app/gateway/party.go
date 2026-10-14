// 本文件为玩家普通组队提供认证后HTTP入口；只做授权上下文、严格契约解析与错误映射，不直接修改领域或存储。
// Actor只来自会话，响应均no-store；未接入扩展的适配器明确503，不回退内存或直接公开内部队伍对象。
package gateway

import (
	"context"
	"net/http"
	"strings"

	"divinebeasts/backend/internal/modules/party"
)

// PartyInteractionPort（可选组队交互端口）由真实HTTP／gRPC客户端实现，保留旧PartyPort源码兼容。
type PartyInteractionPort interface {
	ReadPartyState(context.Context, string) (party.Snapshot, error)
	CommandParty(context.Context, string, party.Command) (party.CommandResult, error)
	GetPartyInvitations(context.Context, string) ([]party.InvitationSnapshot, error)
}

// registerPartyRoutes（注册公开交互）仅注册现有主Gateway，不增加独立登录或社交服务入口。
func (a *api) registerPartyRoutes() {
	a.mux.HandleFunc("GET /v1/party/current", a.requireAuth(a.getCurrentParty))
	a.mux.HandleFunc("GET /v1/party/invitations", a.requireAuth(a.getPartyInvitations))
	a.mux.HandleFunc("POST /v1/party/commands", a.requireAuth(a.commandParty))
}

// getCurrentParty（本人队伍）拒绝通过查询参数指定他人身份。
func (a *api) getCurrentParty(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.party.(PartyInteractionPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	view, err := port.ReadPartyState(r.Context(), session.PlayerID)
	if err != nil {
		writePartyError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, view)
}

// getPartyInvitations（本人收件箱）不允许枚举其他玩家待处理邀请。
func (a *api) getPartyInvitations(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.party.(PartyInteractionPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	items, err := port.GetPartyInvitations(r.Context(), session.PlayerID)
	if err != nil {
		writePartyError(w, err)
		return
	}
	if items == nil {
		items = []party.InvitationSnapshot{}
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, items)
}

// commandParty（提交命令）拒绝重复字段、未知字段、尾随JSON及客户端playerId；命令成功后返回真实提交回执。
func (a *api) commandParty(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.party.(PartyInteractionPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	var command party.Command
	if err := DecodeOnlineJSON(r, &command, []string{"partyId", "action", "targetPlayerId", "membershipRequestId", "clientRequestId", "expectedRevision", "ready"}, []string{"partyId", "action", "clientRequestId", "expectedRevision"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	result, err := port.CommandParty(r.Context(), session.PlayerID, command)
	if err != nil {
		writePartyError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, result)
}

// PartyErrorCode（领域错误白名单）仅透传稳定码；未知存储／解析故障不暴露底层地址或记录正文。
func PartyErrorCode(err error) string {
	if err == nil {
		return ""
	}
	code := strings.SplitN(err.Error(), ":", 2)[0]
	switch code {
	case "PARTY_NOT_LEADER", "PARTY_NOT_MEMBER", "PARTY_REQUEST_FORBIDDEN", "PARTY_NOT_FOUND", "PARTY_REQUEST_NOT_FOUND",
		"PARTY_COMMAND_INVALID", "PARTY_REQUEST_INVALID", "PARTY_REVISION_CONFLICT", "PARTY_IDEMPOTENCY_CONFLICT",
		"PARTY_ROSTER_LOCKED", "PARTY_FULL", "PARTY_ALREADY_MEMBER", "PARTY_MEMBER_UNAVAILABLE", "PARTY_REQUEST_EXPIRED",
		"PARTY_REQUEST_TERMINAL", "PARTY_REQUEST_CONFLICT", "PARTY_REQUEST_PENDING", "PARTY_REQUEST_LIMIT":
		return code
	default:
		return "SERVICE_UNAVAILABLE"
	}
}

// writePartyError（安全错误响应）白名单必须与共享接口状态一致，不能把底层err.Error原文发给客户端。
func writePartyError(w http.ResponseWriter, err error) {
	writeOnlineError(w, ServiceError(PartyErrorCode(err)))
}
