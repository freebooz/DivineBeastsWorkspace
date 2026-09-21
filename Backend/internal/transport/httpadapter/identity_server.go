package httpadapter

import (
	"net/http"

	"divinebeasts/backend/internal/modules/identity"
)

// NewIdentityHandler（创建身份HTTP处理器）暴露内部Login/Authenticate接口。
func NewIdentityHandler(service *identity.Service) http.Handler {
	if service == nil {
		panic("Identity Service不能为空")
	}
	mux := http.NewServeMux()
	mux.HandleFunc("POST /internal/v1/identity/login", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameID     string `json:"gameId"`
			Provider   string `json:"provider"`
			Credential string `json:"credential"`
			DeviceID   string `json:"deviceId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if req.Provider != "guest" {
			writeError(w, http.StatusBadRequest, "AUTH_PROVIDER_UNSUPPORTED", nil)
			return
		}
		session, err := service.LoginGuest(r.Context(), req.GameID, req.DeviceID)
		if err != nil {
			writeError(w, http.StatusUnauthorized, "AUTH_LOGIN_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"playerId": session.PlayerID, "sessionId": session.SessionID, "accessToken": session.AccessToken, "refreshToken": session.RefreshToken, "expiresAtUnixMs": session.AccessExpiresAt.UnixMilli()})
	})
	mux.HandleFunc("POST /internal/v1/identity/authenticate", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			AccessToken string `json:"accessToken"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		session, err := service.Authenticate(r.Context(), req.AccessToken)
		if err != nil {
			writeJSON(w, http.StatusOK, map[string]any{"valid": false, "errorCode": "AUTH_SESSION_INVALID"})
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"valid": true, "playerId": session.PlayerID, "sessionId": session.SessionID})
	})
	return mux
}
