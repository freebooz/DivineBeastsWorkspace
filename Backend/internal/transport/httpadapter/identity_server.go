package httpadapter

import (
	"net/http"

	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/modules/identity"
)

// NewIdentityHandler（创建身份HTTP处理器）暴露内部Login/Authenticate接口。
func NewIdentityHandler(service *identity.Service) http.Handler {
	if service == nil {
		panic("Identity Service不能为空")
	}
	mux := http.NewServeMux()
	registerIdentityOnline(mux, service)
	mux.HandleFunc("POST /internal/v1/identity/login", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			AccountName   string `json:"accountName"`
			ClientVersion string `json:"clientVersion"`
			GameID        string `json:"gameId"`
			Provider      string `json:"provider"`
			Credential    string `json:"credential"`
			DeviceID      string `json:"deviceId"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req, []string{"gameId", "provider", "credential", "deviceId", "accountName", "clientVersion"}, []string{"gameId", "provider", "credential"}); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if req.Provider != "guest" && req.Provider != "password" {
			writeError(w, http.StatusBadRequest, "AUTH_PROVIDER_UNSUPPORTED", nil)
			return
		}
		var session identity.Session
		var err error
		if req.Provider == "password" {
			session, err = service.LoginPassword(r.Context(), req.GameID, req.AccountName, req.Credential, req.DeviceID)
		} else {
			session, err = service.LoginGuest(r.Context(), req.GameID, req.DeviceID)
		}
		if err != nil {
			writeOnlineDomainError(w, err)
			return
		}
		writeIdentitySession(w, session)
	})
	mux.HandleFunc("POST /internal/v1/identity/authenticate", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			AccessToken string `json:"accessToken"`
		}
		if err := gateway.DecodeOnlineJSON(r, &req, []string{"accessToken"}, []string{"accessToken"}); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		session, err := service.Authenticate(r.Context(), req.AccessToken)
		if err != nil {
			status, code := gateway.OnlineErrorStatus(onlineDomainError(err))
			if status == 401 {
				writeJSON(w, http.StatusOK, map[string]any{"valid": false, "errorCode": code})
			} else {
				writeError(w, status, code, nil)
			}
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"valid": true, "playerId": session.PlayerID, "sessionId": session.SessionID})
	})
	return mux
}
