package httpadapter

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"divinebeasts/backend/internal/app/gateway"
)

// ClientConfig（Backend内部HTTP客户端配置）描述下游服务BaseURL和请求超时。
type ClientConfig struct {
	BaseURL string        // BaseURL（服务地址，例如http://identityservice:8081）。
	Timeout time.Duration // Timeout（单次请求超时）。
}

type internalClient struct {
	baseURL string
	client  *http.Client
}

func newInternalClient(cfg ClientConfig) internalClient {
	if cfg.BaseURL == "" {
		panic("Backend内部服务BaseURL不能为空")
	}
	timeout := cfg.Timeout
	if timeout <= 0 {
		timeout = 5 * time.Second
	}
	return internalClient{baseURL: strings.TrimRight(cfg.BaseURL, "/"), client: &http.Client{Timeout: timeout,CheckRedirect:func(*http.Request,[]*http.Request)error{return http.ErrUseLastResponse}}}
}

func (c internalClient) post(ctx context.Context, path string, request, response any) error {
	body, err := json.Marshal(request)
	if err != nil {
		return err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, c.baseURL+path, bytes.NewReader(body))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	return c.do(req, response)
}

func (c internalClient) get(ctx context.Context, path string, response any) error {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, c.baseURL+path, nil)
	if err != nil {
		return err
	}
	return c.do(req, response)
}

func (c internalClient) do(req *http.Request, response any) error {
	resp, err := c.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	payload, err := io.ReadAll(io.LimitReader(resp.Body, maxBodyBytes+1))
	if err != nil {
		return err
	}
	if len(payload)>maxBodyBytes {return gateway.ServiceError("SERVICE_UNAVAILABLE")}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		var problem struct {
			ErrorCode string `json:"errorCode"`
			Message   string `json:"message"`
		}
		_ = json.Unmarshal(payload, &problem)
		status,code:=gateway.OnlineErrorStatus(gateway.ServiceError(problem.ErrorCode));if status!=resp.StatusCode {return gateway.ServiceError("SERVICE_UNAVAILABLE")};return gateway.ServiceError(code)
	}
	if response == nil {
		return nil
	}
	return json.Unmarshal(payload, response)
}

// IdentityClient（身份HTTP客户端）实现Gateway IdentityPort。
type IdentityClient struct{ internalClient }

func NewIdentityClient(cfg ClientConfig) *IdentityClient {
	return &IdentityClient{internalClient: newInternalClient(cfg)}
}

func (c *IdentityClient) Login(ctx context.Context, req gateway.LoginRequest) (gateway.LoginResponse, error) {
	var response struct {
		RefreshExpiresAtUnixMs int64 `json:"refreshExpiresAtUnixMs"`
		PlayerID        string `json:"playerId"`
		SessionID       string `json:"sessionId"`
		AccessToken     string `json:"accessToken"`
		RefreshToken    string `json:"refreshToken"`
		ExpiresAtUnixMs int64  `json:"expiresAtUnixMs"`
	}
	err := c.post(ctx, "/internal/v1/identity/login", req, &response)
	if err != nil {
		return gateway.LoginResponse{}, err
	}
	return gateway.LoginResponse{PlayerID: response.PlayerID, SessionID: response.SessionID, AccessToken: response.AccessToken, RefreshToken: response.RefreshToken, ExpiresAt: time.UnixMilli(response.ExpiresAtUnixMs).UTC().Format(time.RFC3339Nano),RefreshExpiresAt:time.UnixMilli(response.RefreshExpiresAtUnixMs).UTC().Format(time.RFC3339Nano)}, nil
}

func (c *IdentityClient) Authenticate(ctx context.Context, accessToken string) (gateway.AuthenticatedSession, error) {
	var response struct {
		Valid     bool   `json:"valid"`
		PlayerID  string `json:"playerId"`
		SessionID string `json:"sessionId"`
		ErrorCode string `json:"errorCode"`
	}
	if err := c.post(ctx, "/internal/v1/identity/authenticate", map[string]string{"accessToken": accessToken}, &response); err != nil {
		return gateway.AuthenticatedSession{}, err
	}
	if !response.Valid {
		return gateway.AuthenticatedSession{}, gateway.ServiceError(response.ErrorCode)
	}
	return gateway.AuthenticatedSession{PlayerID: response.PlayerID, SessionID: response.SessionID}, nil
}

// PlayerDataClient（玩家资料HTTP客户端）实现Gateway PlayerDataPort。
type PlayerDataClient struct{ internalClient }

func NewPlayerDataClient(cfg ClientConfig) *PlayerDataClient {
	return &PlayerDataClient{internalClient: newInternalClient(cfg)}
}

func (c *PlayerDataClient) GetProfile(ctx context.Context, playerID string) (gateway.PlayerProfile, error) {
	var response struct {
		Found             bool     `json:"found"`
		PlayerID          string   `json:"playerId"`
		GameID            string   `json:"gameId"`
		DisplayName       string   `json:"displayName"`
		DataVersion       int      `json:"dataVersion"`
		Revision          int64    `json:"revision"`
		TutorialCompleted bool     `json:"tutorialCompleted"`
		DefaultWorldID    string   `json:"defaultWorldId"`
		OwnedCharacterIDs []string `json:"ownedCharacterIds"`
	}
	if err := c.get(ctx, "/internal/v1/playerdata/profile?playerId="+url.QueryEscape(playerID), &response); err != nil {
		return gateway.PlayerProfile{}, err
	}
	if !response.Found {
		return gateway.PlayerProfile{}, gateway.ServiceError("PLAYER_PROFILE_NOT_FOUND")
	}
	return gateway.PlayerProfile{PlayerID: response.PlayerID, GameID: response.GameID, DisplayName: response.DisplayName, DataVersion: response.DataVersion, Revision: response.Revision, TutorialCompleted: response.TutorialCompleted, DefaultWorldID: response.DefaultWorldID, OwnedCharacterIDs: response.OwnedCharacterIDs}, nil
}

// MatchClient（MatchService HTTP客户端）同时实现Gateway PartyPort和MatchmakingPort。
type MatchClient struct{ internalClient }

func NewMatchClient(cfg ClientConfig) *MatchClient {
	return &MatchClient{internalClient: newInternalClient(cfg)}
}

func (c *MatchClient) CreateParty(ctx context.Context, playerID string) (gateway.PartySnapshot, error) {
	var response gateway.PartySnapshot
	if err := c.post(ctx, "/internal/v1/match/party", map[string]string{"playerId": playerID, "displayName": ""}, &response); err != nil {
		return gateway.PartySnapshot{}, err
	}
	return response, nil
}

func (c *MatchClient) CreateTicket(ctx context.Context, playerID string, req gateway.CreateMatchmakingTicketRequest) (gateway.MatchmakingTicketResponse, error) {
	var response gateway.MatchmakingTicketResponse
	if err := c.post(ctx, "/internal/v1/match/tickets", map[string]any{"playerId": playerID, "arenaModeId": req.ArenaModeID, "partyId": req.PartyID, "preferredRegion": req.PreferredRegion, "clientRequestId": req.ClientRequestID}, &response); err != nil {
		return gateway.MatchmakingTicketResponse{}, err
	}
	return response, nil
}
