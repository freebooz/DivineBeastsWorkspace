package httpadapter

import (
	"bytes"
	"context"
	"divinebeasts/backend/internal/app/gateway"
	"encoding/json"
	"net/http"
	"time"
)

// Probe 必须取得业务ready=true；空成功正文不能冒充可用。
func (c *IdentityClient) Probe(ctx context.Context) error {
	return c.probe(ctx, "/internal/v1/identity/probe")
}
func (c *PlayerDataClient) Probe(ctx context.Context) error {
	return c.probe(ctx, "/internal/v1/playerdata/probe")
}
func (c internalClient) probe(ctx context.Context, path string) error {
	var out struct {
		Ready bool `json:"ready"`
	}
	if err := c.get(ctx, path, &out); err != nil {
		return err
	}
	if !out.Ready {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return nil
}

// Refresh 不进行自动重试，响应丢失可能已消费旧凭据。
func (c *IdentityClient) Refresh(ctx context.Context, token string) (gateway.LoginResponse, error) {
	var out struct {
		PlayerID       string `json:"playerId"`
		SessionID      string `json:"sessionId"`
		AccessToken    string `json:"accessToken"`
		RefreshToken   string `json:"refreshToken"`
		Expires        int64  `json:"expiresAtUnixMs"`
		RefreshExpires int64  `json:"refreshExpiresAtUnixMs"`
	}
	if err := c.post(ctx, "/internal/v1/identity/refresh", map[string]string{"refreshToken": token}, &out); err != nil {
		return gateway.LoginResponse{}, err
	}
	if out.PlayerID == "" || out.SessionID == "" || out.AccessToken == "" || out.RefreshToken == "" || out.Expires <= 0 || out.RefreshExpires <= 0 {
		return gateway.LoginResponse{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return gateway.LoginResponse{PlayerID: out.PlayerID, SessionID: out.SessionID, AccessToken: out.AccessToken, RefreshToken: out.RefreshToken, ExpiresAt: time.UnixMilli(out.Expires).UTC().Format(time.RFC3339Nano), RefreshExpiresAt: time.UnixMilli(out.RefreshExpires).UTC().Format(time.RFC3339Nano)}, nil
}

// Logout 只有下游成功响应才确认撤销。
func (c *IdentityClient) Logout(ctx context.Context, token string) error {
	return c.post(ctx, "/internal/v1/identity/logout", map[string]string{"refreshToken": token}, nil)
}

// EnsureProfile 只通过所属服务初始化，不在网关直写数据库。
func (c *PlayerDataClient) EnsureProfile(ctx context.Context, id, game string) error {
	return c.post(ctx, "/internal/v1/playerdata/ensure", map[string]string{"playerId": id, "gameId": game}, nil)
}
func (c *PlayerDataClient) UpdateDisplayNameIdempotent(ctx context.Context, id, name string, revision int64, key string) (gateway.PlayerProfile, error) {
	payload, err := json.Marshal(map[string]any{"playerId": id, "displayName": name, "expectedRevision": revision, "idempotencyKey": key})
	if err != nil {
		return gateway.PlayerProfile{}, err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPatch, c.baseURL+"/internal/v1/playerdata/profile", bytes.NewReader(payload))
	if err != nil {
		return gateway.PlayerProfile{}, err
	}
	req.Header.Set("Content-Type", "application/json")
	var out struct {
		gateway.PlayerProfile
		Found bool `json:"found"`
	}
	if err = c.do(req, &out); err != nil {
		return gateway.PlayerProfile{}, err
	}
	if !out.Found || out.PlayerID != id || out.DataVersion < 1 || out.Revision < 0 {
		return gateway.PlayerProfile{}, gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return out.PlayerProfile, nil
}
