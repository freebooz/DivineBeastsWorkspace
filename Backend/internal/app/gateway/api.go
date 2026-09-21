// Package gateway（统一接入应用层）提供Game Client面向Backend的HTTP REST协议适配。
// Gateway只处理认证上下文、请求追踪、JSON编解码和下游端口调用，不实现身份、玩家数据、Party或匹配领域规则。
package gateway

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"strings"
)

// ErrUnauthorized（未认证错误）用于统一把无效Access Token映射为HTTP 401。
var ErrUnauthorized = errors.New("AUTH_SESSION_INVALID: 玩家会话无效")

// Config（Gateway接口配置）保存跨请求共享的协议版本信息。
type Config struct {
	ContractVersion string // ContractVersion（Shared Contract共享契约版本）。
}

// LoginRequest（登录请求）与Shared OpenAPI中的客户端登录字段保持一致。
type LoginRequest struct {
	GameID        string `json:"gameId"`        // GameID（游戏ID）。
	Provider      string `json:"provider"`      // Provider（登录提供方，如guest/platform）。
	Credential    string `json:"credential"`    // Credential（登录凭据）。
	ClientVersion string `json:"clientVersion"` // ClientVersion（游戏客户端版本）。
	DeviceID      string `json:"deviceId"`      // DeviceID（客户端设备标识）。
}

// LoginResponse（登录响应）返回后续业务请求所需的玩家与Token上下文。
type LoginResponse struct {
	PlayerID     string `json:"playerId"`     // PlayerID（玩家ID）。
	AccessToken  string `json:"accessToken"`  // AccessToken（短期访问令牌）。
	RefreshToken string `json:"refreshToken"` // RefreshToken（刷新令牌）。
	ExpiresAt    string `json:"expiresAt"`    // ExpiresAt（访问令牌过期时间，ISO-8601）。
	SessionID    string `json:"sessionId"`    // SessionID（在线会话ID）。
}

// AuthenticatedSession（已认证会话）是Gateway从Access Token解析得到的可信调用者身份。
type AuthenticatedSession struct {
	PlayerID  string // PlayerID（已认证玩家ID）。
	SessionID string // SessionID（已认证在线会话ID）。
}

// PlayerProfile（玩家资料响应）只包含跨局长期数据，不包含实时Gameplay状态。
type PlayerProfile struct {
	PlayerID          string   `json:"playerId"`          // PlayerID（玩家ID）。
	GameID            string   `json:"gameId"`            // GameID（游戏ID）。
	DisplayName       string   `json:"displayName"`       // DisplayName（玩家显示名称）。
	DataVersion       int      `json:"dataVersion"`       // DataVersion（资料结构版本）。
	Revision          int64    `json:"revision"`          // Revision（乐观并发版本）。
	TutorialCompleted bool     `json:"tutorialCompleted"` // TutorialCompleted（是否完成新手教学）。
	DefaultWorldID    string   `json:"defaultWorldId"`    // DefaultWorldID（默认进入世界ID）。
	OwnedCharacterIDs []string `json:"ownedCharacterIds"` // OwnedCharacterIDs（已拥有角色ID列表）。
}

// PartySnapshot（Party快照）是Gateway返回给Game Client的只读组队状态。
type PartySnapshot struct {
	PartyID             string   `json:"partyId"`             // PartyID（组队ID）。
	LeaderPlayerID      string   `json:"leaderPlayerId"`      // LeaderPlayerID（队长玩家ID）。
	MemberPlayerIDs     []string `json:"memberPlayerIds"`     // MemberPlayerIDs（成员玩家ID列表）。
	SelectedArenaModeID string   `json:"selectedArenaModeId"` // SelectedArenaModeID（当前选择竞技模式）。
	RosterLocked        bool     `json:"rosterLocked"`        // RosterLocked（名单是否已锁定）。
	Revision            int64    `json:"revision"`            // Revision（Party修订版本）。
}

// CreateMatchmakingTicketRequest（创建匹配票据请求）只接受客户端有权声明的字段。
type CreateMatchmakingTicketRequest struct {
	ArenaModeID     string `json:"arenaModeId"`     // ArenaModeID（1v1至5v5竞技模式ID）。
	PartyID         string `json:"partyId"`         // PartyID（组队ID，单排可为空）。
	PreferredRegion string `json:"preferredRegion"` // PreferredRegion（首选匹配区域）。
	ClientRequestID string `json:"clientRequestId"` // ClientRequestID（客户端幂等请求ID）。
}

// MatchmakingTicketResponse（匹配票据响应）描述当前匹配原子单元状态。
type MatchmakingTicketResponse struct {
	TicketID       string   `json:"ticketId"`       // TicketID（匹配票据ID）。
	ArenaModeID    string   `json:"arenaModeId"`    // ArenaModeID（竞技模式ID）。
	PartyID        string   `json:"partyId"`        // PartyID（组队ID，单排为空）。
	PartyMemberIDs []string `json:"partyMemberIds"` // PartyMemberIDs（不可拆分玩家ID列表）。
	PartySize      int      `json:"partySize"`      // PartySize（匹配原子单元人数）。
	TeamSize       int      `json:"teamSize"`       // TeamSize（竞技模式每队人数）。
	State          string   `json:"state"`          // State（匹配状态）。
}

// IdentityPort（身份服务端口）定义Gateway依赖的最小身份能力。
type IdentityPort interface {
	Login(ctx context.Context, req LoginRequest) (LoginResponse, error)
	Authenticate(ctx context.Context, accessToken string) (AuthenticatedSession, error)
}

// PlayerDataPort（玩家数据服务端口）定义Gateway读取长期玩家资料所需能力。
type PlayerDataPort interface {
	GetProfile(ctx context.Context, playerID string) (PlayerProfile, error)
}

// PartyPort（Party服务端口）定义Gateway创建Party所需能力。
type PartyPort interface {
	CreateParty(ctx context.Context, playerID string) (PartySnapshot, error)
}

// MatchmakingPort（匹配服务端口）定义Gateway创建匹配票据所需能力。
type MatchmakingPort interface {
	CreateTicket(ctx context.Context, playerID string, req CreateMatchmakingTicketRequest) (MatchmakingTicketResponse, error)
}

// api（Gateway HTTP处理器）持有下游应用端口并负责协议适配。
type api struct {
	config      Config
	identity    IdentityPort
	playerData  PlayerDataPort
	party       PartyPort
	matchmaking MatchmakingPort
	mux         *http.ServeMux
}

// NewAPI（创建Gateway HTTP API）构建REST路由和统一请求追踪中间件。
func NewAPI(config Config, identity IdentityPort, playerData PlayerDataPort, party PartyPort, matchmaking MatchmakingPort) http.Handler {
	if identity == nil || playerData == nil || party == nil || matchmaking == nil {
		panic("Gateway下游端口不能为空")
	}
	handler := &api{config: config, identity: identity, playerData: playerData, party: party, matchmaking: matchmaking, mux: http.NewServeMux()}
	handler.mux.HandleFunc("POST /v1/auth/login", handler.login)
	handler.mux.HandleFunc("GET /v1/player/profile", handler.requireAuth(handler.getProfile))
	handler.mux.HandleFunc("POST /v1/party", handler.requireAuth(handler.createParty))
	handler.mux.HandleFunc("POST /v1/matchmaking/tickets", handler.requireAuth(handler.createMatchmakingTicket))
	handler.registerSwaggerDocumentation(os.Getenv(swaggerContractsRootEnvironment))
	return handler.tracing(handler.mux)
}

// tracing（请求追踪中间件）统一生成或透传RequestId和TraceId，并返回Shared Contract版本。
func (a *api) tracing(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestID := strings.TrimSpace(r.Header.Get("X-Request-Id"))
		if requestID == "" {
			requestID = randomID("req")
		}
		traceID := strings.TrimSpace(r.Header.Get("X-Trace-Id"))
		if traceID == "" {
			traceID = randomID("trace")
		}
		w.Header().Set("X-Request-Id", requestID)
		w.Header().Set("X-Trace-Id", traceID)
		if a.config.ContractVersion != "" {
			w.Header().Set("X-Contract-Version", a.config.ContractVersion)
		}
		next.ServeHTTP(w, r)
	})
}

// requireAuth（Bearer认证中间件）从Authorization头解析Access Token，并把可信Session写入Context。
func (a *api) requireAuth(next func(http.ResponseWriter, *http.Request, AuthenticatedSession)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		header := strings.TrimSpace(r.Header.Get("Authorization"))
		const prefix = "Bearer "
		if !strings.HasPrefix(header, prefix) || strings.TrimSpace(strings.TrimPrefix(header, prefix)) == "" {
			writeAPIError(w, http.StatusUnauthorized, "AUTH_SESSION_INVALID", "缺少有效Bearer Access Token")
			return
		}
		session, err := a.identity.Authenticate(r.Context(), strings.TrimSpace(strings.TrimPrefix(header, prefix)))
		if err != nil {
			writeAPIError(w, http.StatusUnauthorized, "AUTH_SESSION_INVALID", "玩家会话无效")
			return
		}
		next(w, r, session)
	}
}

func (a *api) login(w http.ResponseWriter, r *http.Request) {
	var req LoginRequest
	if err := decodeJSON(r, &req); err != nil {
		writeAPIError(w, http.StatusBadRequest, "INVALID_REQUEST", "登录请求JSON无效")
		return
	}
	response, err := a.identity.Login(r.Context(), req)
	if err != nil {
		writeAPIError(w, http.StatusUnauthorized, "AUTH_INVALID_CREDENTIALS", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, response)
}

func (a *api) getProfile(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	profile, err := a.playerData.GetProfile(r.Context(), session.PlayerID)
	if err != nil {
		writeAPIError(w, http.StatusNotFound, "PLAYER_PROFILE_NOT_FOUND", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, profile)
}

func (a *api) createParty(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	snapshot, err := a.party.CreateParty(r.Context(), session.PlayerID)
	if err != nil {
		writeAPIError(w, http.StatusConflict, "PARTY_CREATE_FAILED", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, snapshot)
}

func (a *api) createMatchmakingTicket(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	var req CreateMatchmakingTicketRequest
	if err := decodeJSON(r, &req); err != nil {
		writeAPIError(w, http.StatusBadRequest, "INVALID_REQUEST", "匹配请求JSON无效")
		return
	}
	ticket, err := a.matchmaking.CreateTicket(r.Context(), session.PlayerID, req)
	if err != nil {
		writeAPIError(w, http.StatusConflict, "MATCH_CREATE_TICKET_FAILED", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, ticket)
}

func decodeJSON(r *http.Request, target any) error {
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	return decoder.Decode(target)
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

func writeAPIError(w http.ResponseWriter, status int, errorCode, message string) {
	writeJSON(w, status, map[string]any{"errorCode": errorCode, "message": message, "retryable": false})
}

func randomID(prefix string) string {
	var raw [12]byte
	if _, err := rand.Read(raw[:]); err != nil {
		return prefix + "-fallback"
	}
	return prefix + "-" + hex.EncodeToString(raw[:])
}
