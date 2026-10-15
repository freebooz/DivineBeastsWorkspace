// 本文件是公网匹配生命周期适配：认证主体与已选角色由后端验证，Widget只发送意图，不接受客户端地址或比赛结果。
package gateway

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"net/http"
	"strings"
	"time"
)

// MatchmakingLifecyclePort由同一Match服务HTTP/gRPC客户端实现，保持五薄入口；未装配时明确503。
type MatchmakingLifecyclePort interface {
	ReadMatchmakingState(context.Context, string) (matchmaking.PlayerSnapshot, error)
	ObserveMatchmaking(context.Context, string, string, time.Duration) (matchmaking.Observation, error)
	CommandMatchmaking(context.Context, matchmaking.ActorContext, matchmaking.Command) (matchmaking.PlayerSnapshot, error)
	GetMatchmakingMigration(context.Context, matchmaking.ActorContext, matchmaking.MigrationRequest) (matchmaking.Migration, error)
}

// registerMatchmakingRoutes仅注册主Gateway认证路由，内部服务不接受公网连接自行提交actor。
func (a *api) registerMatchmakingRoutes() {
	a.mux.HandleFunc("POST /v1/matchmaking/commands", a.requireAuth(a.commandMatchmaking))
	a.mux.HandleFunc("POST /v1/matchmaking/observe", a.requireAuth(a.observeMatchmaking))
	a.mux.HandleFunc("POST /v1/matchmaking/migration", a.requireAuth(a.getMatchmakingMigration))
}

// authenticatedMatchmakingActor复核角色真实归属与当前选择；取消/普通观察不依赖角色仍存在，避免无法清理锁。
func (a *api) authenticatedMatchmakingActor(ctx context.Context, session AuthenticatedSession, requireCharacter bool) (matchmaking.ActorContext, error) {
	actor := matchmaking.ActorContext{PlayerID: session.PlayerID, SessionID: session.SessionID}
	if !requireCharacter {
		return actor, nil
	}
	profile, err := a.playerData.GetProfile(ctx, session.PlayerID)
	if err != nil {
		return actor, err
	}
	characters, ok := a.playerData.(CharacterPort)
	if !ok {
		return actor, ServiceError("SERVICE_UNAVAILABLE")
	}
	items, err := characters.ListCharacters(ctx, session.PlayerID)
	if err != nil {
		return actor, err
	}
	for _, character := range items {
		if character.CharacterID == profile.SelectedCharacterID && character.CharacterID != "" && character.Status == "Active" && character.CharacterRevision > 0 {
			actor.CharacterID = character.CharacterID
			actor.HeroDefinitionID = character.HeroDefinitionID
			actor.CharacterRevision = character.CharacterRevision
			return actor, nil
		}
	}
	return actor, ServiceError("MATCH_CHARACTER_REQUIRED")
}

// commandMatchmaking严格白名单解析并保留原请求键；不信任客户端PlayerID、SessionID、CharacterID或隐藏分。
func (a *api) commandMatchmaking(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.matchmaking.(MatchmakingLifecyclePort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	var command matchmaking.Command
	if err := DecodeOnlineJSON(r, &command, []string{"action", "clientRequestId", "expectedRevision", "ticketId", "candidateId", "arenaModeId", "partyId", "regionId", "accept"}, []string{"action", "clientRequestId", "expectedRevision"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	actor, err := a.authenticatedMatchmakingActor(r.Context(), session, command.Action == "start" || command.Action == "reply")
	if err != nil {
		writeMatchmakingError(w, err)
		return
	}
	if command.Action == "start" || command.Action == "reply" {
		if err := a.attachMatchmakingPresence(r.Context(), &actor); err != nil {
			writeMatchmakingError(w, err)
			return
		}
	}
	state, err := port.CommandMatchmaking(r.Context(), actor, command)
	if err != nil {
		writeMatchmakingError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, map[string]any{"clientRequestId": command.ClientRequestID, "snapshot": state})
}

// observeMatchmaking只等待本人变化；空响应保留状态，不能以网络失败伪造idle。
func (a *api) observeMatchmaking(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.matchmaking.(MatchmakingLifecyclePort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	var request struct {
		Cursor           string `json:"cursor"`
		WaitMilliseconds int64  `json:"waitMilliseconds"`
	}
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	if err := DecodeOnlineJSON(r, &request, []string{"cursor", "waitMilliseconds"}, []string{"cursor", "waitMilliseconds"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	if request.WaitMilliseconds < 0 || request.WaitMilliseconds > 20000 || len(request.Cursor) > 43 {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	state, err := port.ObserveMatchmaking(r.Context(), session.PlayerID, request.Cursor, time.Duration(request.WaitMilliseconds)*time.Millisecond)
	if err != nil {
		writeMatchmakingError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, state)
}

// getMatchmakingMigration只向当前会话返回本人新准入；公开队伍快照和观察永远不包含此秘密。
func (a *api) getMatchmakingMigration(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	port, ok := a.matchmaking.(MatchmakingLifecyclePort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	var request matchmaking.MigrationRequest
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	if err := DecodeOnlineJSON(r, &request, []string{"candidateId", "direction", "clientRequestId"}, []string{"candidateId", "direction", "clientRequestId"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	actor, err := a.authenticatedMatchmakingActor(r.Context(), session, true)
	if err != nil {
		writeMatchmakingError(w, err)
		return
	}
	state, err := port.GetMatchmakingMigration(r.Context(), actor, request)
	if err != nil {
		writeMatchmakingError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, state)
}

// MatchmakingErrorCode只传播稳定安全码，控制面地址/数据库异常及任意内部message都不进入客户端。
func MatchmakingErrorCode(err error) string {
	if err == nil {
		return ""
	}
	code := strings.SplitN(err.Error(), ":", 2)[0]
	switch code {
	case "MATCH_WORLD_REQUIRED", "MATCH_COMMAND_REQUIRED", "MATCH_COMMAND_INVALID", "MATCH_MODE_INVALID", "MATCH_PARTY_TOO_LARGE", "MATCH_FORBIDDEN", "MATCH_NOT_FOUND", "MATCH_REVISION_CONFLICT", "MATCH_IDEMPOTENCY_CONFLICT", "MATCH_PHASE_CONFLICT", "MATCH_DEADLINE_EXPIRED", "MATCH_PLAYER_RESERVED", "MATCH_PARTY_REQUIRED", "MATCH_PARTY_NOT_READY", "MATCH_CHARACTER_REQUIRED", "MATCH_QUEUE_CAPACITY":
		return code
	default:
		if code == "INVALID_REQUEST" {
			return code
		}
		return PartyErrorCode(err)
	}
}
func writeMatchmakingError(w http.ResponseWriter, err error) {
	writeOnlineError(w, ServiceError(MatchmakingErrorCode(err)))
}
