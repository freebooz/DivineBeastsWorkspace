package httpadapter

import (
	"errors"
	"net/http"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
)

// registerArenaStartAbort使用既有专服控制面认证，比赛请求体不能替代实例身份头或权威分配。
func registerArenaStartAbort(mux *http.ServeMux, service *gameservercontrol.Service, bearer string) {
	mux.Handle("POST /internal/v1/gameservers/match-start-abort", requireGameServerControlBearer(bearer, func(w http.ResponseWriter, r *http.Request) {
		var input gameservercontrol.ArenaStartAbortInput
		fields := []string{"matchId", "gameServerId", "serverBootId", "reason"}
		if err := gateway.DecodeOnlineJSON(r, &input, fields, fields); err != nil {
			writeArenaLifecycleError(w, err)
			return
		}
		if !requireGameServerIdentity(w, r, input.GameServerID, input.ServerBootID) {
			return
		}
		switch err := service.ReportArenaStartAbort(r.Context(), input); {
		case errors.Is(err, gameservercontrol.ErrArenaStartAbortInvalid):
			writeError(w, http.StatusBadRequest, "ARENA_START_ABORT_INVALID", nil)
		case errors.Is(err, gameservercontrol.ErrArenaStartAbortConflict):
			writeError(w, http.StatusConflict, "ARENA_START_ABORT_CONFLICT", nil)
		case err != nil:
			writeError(w, http.StatusServiceUnavailable, "ARENA_START_ABORT_RETRYABLE", nil)
		default:
			writeJSON(w, http.StatusOK, map[string]bool{"accepted": true})
		}
	}))
}
