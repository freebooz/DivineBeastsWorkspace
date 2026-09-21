package httpadapter

import (
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/identity"
	"net/http"
)

// registerIdentityOnline 仅供可信内部连接；不以宿主health替代仓储Probe。
func registerIdentityOnline(mux *http.ServeMux, service *identity.Service) {
	mux.HandleFunc("GET /internal/v1/identity/probe", func(w http.ResponseWriter, r *http.Request) {
		if err := service.Probe(r.Context()); err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeJSON(w, 200, map[string]bool{"ready": true})
	})
	mux.HandleFunc("POST /internal/v1/identity/refresh", func(w http.ResponseWriter, r *http.Request) {
		token, ok := internalRefreshToken(w, r)
		if !ok {
			return
		}
		session, err := service.Refresh(r.Context(), token)
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeIdentitySession(w, session)
	})
	mux.HandleFunc("POST /internal/v1/identity/logout", func(w http.ResponseWriter, r *http.Request) {
		token, ok := internalRefreshToken(w, r)
		if !ok {
			return
		}
		if err := service.Logout(r.Context(), token); err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		w.WriteHeader(204)
	})
}
func internalRefreshToken(w http.ResponseWriter, r *http.Request) (string, bool) {
	var req struct {
		RefreshToken string `json:"refreshToken"`
	}
	err := gateway.DecodeOnlineJSON(r, &req, []string{"refreshToken"}, []string{"refreshToken"})
	if err != nil || req.RefreshToken == "" {
		writeError(w, 400, "INVALID_REQUEST", nil)
		return "", false
	}
	return req.RefreshToken, true
}
func writeIdentitySession(w http.ResponseWriter, session identity.Session) {
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, 200, map[string]any{"playerId": session.PlayerID, "sessionId": session.SessionID, "accessToken": session.AccessToken, "refreshToken": session.RefreshToken, "expiresAtUnixMs": session.AccessExpiresAt.UnixMilli(), "refreshExpiresAtUnixMs": session.RefreshExpiresAt.UnixMilli()})
}
