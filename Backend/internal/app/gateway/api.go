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
	ContractVersion       string // ContractVersion（Shared Contract共享契约版本）。
	AuthRequestsPerMinute int    // 登录/刷新每源IP每分钟上限；零使用30，不信任客户端转发头。
}

// LoginRequest（登录请求）与Shared OpenAPI中的客户端登录字段保持一致。
type LoginRequest struct {
	AccountName   string `json:"accountName,omitempty"` // 密码提供方账号名；原credential字段继续承载密码。
	GameID        string `json:"gameId"`                // GameID（游戏ID）。
	Provider      string `json:"provider"`              // Provider（登录提供方，如guest/platform）。
	Credential    string `json:"credential"`            // Credential（登录凭据）。
	ClientVersion string `json:"clientVersion"`         // ClientVersion（游戏客户端版本）。
	DeviceID      string `json:"deviceId"`              // DeviceID（客户端设备标识）。
}

// LoginResponse（登录响应）返回后续业务请求所需的玩家与Token上下文。
type LoginResponse struct {
	RefreshExpiresAt string `json:"refreshExpiresAt"` // 刷新凭据绝对到期，UTC RFC3339。
	PlayerID         string `json:"playerId"`         // PlayerID（玩家ID）。
	AccessToken      string `json:"accessToken"`      // AccessToken（短期访问令牌）。
	RefreshToken     string `json:"refreshToken"`     // RefreshToken（刷新令牌）。
	ExpiresAt        string `json:"expiresAt"`        // ExpiresAt（访问令牌过期时间，ISO-8601）。
	SessionID        string `json:"sessionId"`        // SessionID（在线会话ID）。
}

// AuthenticatedSession（已认证会话）是Gateway从Access Token解析得到的可信调用者身份。
type AuthenticatedSession struct {
	PlayerID  string // PlayerID（已认证玩家ID）。
	SessionID string // SessionID（已认证在线会话ID）。
}

// PlayerProfile（玩家资料响应）只包含跨局长期数据，不包含实时Gameplay状态。
type PlayerProfile struct {
	PlayerID            string   `json:"playerId"`            // PlayerID（玩家ID）。
	GameID              string   `json:"gameId"`              // GameID（游戏ID）。
	DisplayName         string   `json:"displayName"`         // DisplayName（玩家显示名称）。
	DataVersion         int      `json:"dataVersion"`         // DataVersion（资料结构版本）。
	Revision            int64    `json:"revision"`            // Revision（乐观并发版本）。
	TutorialCompleted   bool     `json:"tutorialCompleted"`   // TutorialCompleted（是否完成新手教学）。
	DefaultWorldID      string   `json:"defaultWorldId"`      // DefaultWorldID（默认进入世界ID）。
	SelectedCharacterID string   `json:"selectedCharacterId"` // SelectedCharacterID（最近一次权威选择角色ID）。
	OwnedCharacterIDs   []string `json:"ownedCharacterIds"`   // OwnedCharacterIDs（已拥有角色ID列表）。
}

// CharacterSummary（持久角色摘要）用于登录后的角色选择，不等于MainArena竞技选人。
type CharacterSummary struct {
	CharacterID         string `json:"characterId"`
	HeroDefinitionID    string `json:"heroDefinitionId"`
	CharacterName       string `json:"characterName"`
	CharacterRevision   int64  `json:"characterRevision"`
	OnboardingState     string `json:"onboardingState"`
	Status              string `json:"status"`
	AppearanceProfileID string `json:"appearanceProfileId,omitempty"`
}

// CreateCharacterRequest（创建持久角色请求）不包含PlayerID，身份必须来自Bearer会话。
type CreateCharacterRequest struct {
	CreationRequestID   string            `json:"creationRequestId"`
	HeroDefinitionID    string            `json:"heroDefinitionId"`
	CharacterName       string            `json:"characterName"`
	AppearanceSelection map[string]string `json:"appearanceSelection,omitempty"`
}

// CharacterSelectionRequest（角色选择请求）携带客户端列表快照中的Revision用于并发校验。
type CharacterSelectionRequest struct {
	SelectionRequestID        string `json:"selectionRequestId"`
	CharacterID               string `json:"characterId"`
	ExpectedCharacterRevision int64  `json:"expectedCharacterRevision"`
}

// CharacterSelectionResponse（权威角色选择响应）返回资料新Revision和服务端确认角色。
type CharacterSelectionResponse struct {
	SelectionRequestID string           `json:"selectionRequestId"`
	ProfileRevision    int64            `json:"profileRevision"`
	Character          CharacterSummary `json:"character"`
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

// WorldEntryRequest（世界进入请求）描述已认证玩家进入常驻世界所需的项目业务上下文。
// PlayerID、SessionID和GameID均从服务端认证/资料上下文取得，客户端不得自行声明。
type WorldEntryRequest struct {
	RequestID                 string `json:"requestId"`
	CharacterID               string `json:"characterId"`
	DesiredExperienceID       string `json:"desiredExperienceId"`
	ExpectedCharacterRevision int64  `json:"expectedCharacterRevision"`
	PreferredRegion           string `json:"preferredRegion,omitempty"`
}

// WorldEntryResponse（世界进入响应）只向客户端返回当前一次迁移所需Assignment摘要和一次性TransferTicket。
// TransferTicket不得写日志、URL或长期持久化；客户端消费后应立即交给Session传输层。
type WorldEntryResponse struct {
	AssignmentID   string `json:"assignmentId"`
	GameServerID   string `json:"gameServerId"`
	ServerRoleID   string `json:"serverRoleId"`
	ExperienceID   string `json:"experienceId"`
	WorldID        string `json:"worldId"`
	MapID          string `json:"mapId"`
	RegionID       string `json:"regionId"`
	TicketID       string `json:"ticketId"`
	CharacterID    string `json:"characterId"`
	SessionID      string `json:"sessionId"`
	Endpoint       string `json:"endpoint"`
	TransferTicket string `json:"transferTicket"`
}

// WorldEntryAllocationRequest（世界分配内部请求）仅在Gateway到GameServerControl可信服务间传递。
type WorldEntryAllocationRequest struct {
	RequestID           string
	GameID              string
	PlayerID            string
	SessionID           string
	CharacterID         string
	DesiredExperienceID string
	PreferredRegion     string
}

// WorldEntryPort（世界进入端口）隔离Gateway与具体GameServerControl HTTP/gRPC传输实现。
type WorldEntryPort interface {
	AllocateWorldEntry(context.Context, WorldEntryAllocationRequest) (WorldEntryResponse, error)
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

// CharacterPort（持久角色端口）是PlayerDataService对Gateway暴露的角色长期数据能力。
type CharacterPort interface {
	ListCharacters(ctx context.Context, playerID string) ([]CharacterSummary, error)
	CreateCharacter(ctx context.Context, playerID string, req CreateCharacterRequest) (CharacterSummary, error)
	SelectCharacter(ctx context.Context, playerID string, req CharacterSelectionRequest) (CharacterSelectionResponse, error)
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
	worldEntry  WorldEntryPort
	mux         *http.ServeMux
	authLimiter *authRateLimiter
}

// NewAPI（创建Gateway HTTP API）构建REST路由和统一请求追踪中间件。
func NewAPI(config Config, identity IdentityPort, playerData PlayerDataPort, party PartyPort, matchmaking MatchmakingPort, worldEntry ...WorldEntryPort) http.Handler {
	if identity == nil || playerData == nil {
		panic("Gateway下游端口不能为空")
	}
	var worldEntryPort WorldEntryPort
	if len(worldEntry) > 0 {
		worldEntryPort = worldEntry[0]
	}
	handler := &api{config: config, identity: identity, playerData: playerData, party: party, matchmaking: matchmaking, worldEntry: worldEntryPort, mux: http.NewServeMux()}
	handler.authLimiter = newAuthRateLimiter(config.AuthRequestsPerMinute)
	handler.mux.HandleFunc("GET /v1/online/probe", handler.probeOnline)
	handler.mux.HandleFunc("POST /v1/auth/refresh", handler.refreshOnline)
	handler.mux.HandleFunc("POST /v1/auth/logout", handler.logoutOnline)
	handler.mux.HandleFunc("PATCH /v1/player/profile", handler.requireAuth(handler.updateProfile))
	handler.mux.HandleFunc("POST /v1/auth/login", handler.login)
	handler.mux.HandleFunc("GET /v1/player/profile", handler.requireAuth(handler.getProfile))
	handler.mux.HandleFunc("GET /v1/player/characters", handler.requireAuth(handler.listCharacters))
	handler.mux.HandleFunc("POST /v1/player/characters", handler.requireAuth(handler.createCharacter))
	handler.mux.HandleFunc("POST /v1/player/character-selection", handler.requireAuth(handler.selectCharacter))
	handler.mux.HandleFunc("POST /v1/party", handler.requireAuth(handler.createParty))
	handler.mux.HandleFunc("POST /v1/matchmaking/tickets", handler.requireAuth(handler.createMatchmakingTicket))
	handler.mux.HandleFunc("POST /v1/divinebeasts/world-entry", handler.requireAuth(handler.enterDivineBeastsWorld))
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
			writeOnlineError(w, err)
			return
		}
		if session.PlayerID == "" || session.SessionID == "" {
			writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
			return
		}
		next(w, r, session)
	}
}

func (a *api) login(w http.ResponseWriter, r *http.Request) {
	if !a.allowAuthentication(w, r) {
		return
	}
	var req LoginRequest
	if err := DecodeOnlineJSON(r, &req, []string{"gameId", "provider", "credential", "clientVersion", "accountName", "deviceId"}, []string{"gameId", "provider", "credential", "clientVersion"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	if strings.TrimSpace(req.GameID) == "" || strings.TrimSpace(req.ClientVersion) == "" || req.Credential == "" || (req.Provider != "password" && req.Provider != "guest") || (req.Provider == "password" && strings.TrimSpace(req.AccountName) == "") {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	response, err := a.identity.Login(r.Context(), req)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	// Identity成功返回的PlayerID才允许进入PlayerData建档。生产环境password是正式提供方；
	// 本地legacy guest如果被Identity显式接受，也使用同一正式建档用例，不再依赖读取时隐式创建。
	initializer, ok := a.playerData.(ProfileInitializer)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	if err := initializer.EnsureProfile(r.Context(), response.PlayerID, req.GameID); err != nil {
		// 建档属于PlayerData用例；失败不返回令牌，并尽力撤销刚创建的身份会话。
		if lifecycle, ok := a.identity.(AuthenticationLifecycle); ok {
			_ = lifecycle.Logout(r.Context(), response.RefreshToken)
		}
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, response)
}

func (a *api) getProfile(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	profile, err := a.playerData.GetProfile(r.Context(), session.PlayerID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	writeProfile(w, profile)
}

func (a *api) listCharacters(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	port, ok := a.playerData.(CharacterPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	characters, err := port.ListCharacters(r.Context(), session.PlayerID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	if characters == nil {
		characters = []CharacterSummary{}
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, characters)
}

func (a *api) createCharacter(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	var req CreateCharacterRequest
	if err := DecodeOnlineJSON(r, &req,
		[]string{"creationRequestId", "heroDefinitionId", "characterName", "appearanceSelection"},
		[]string{"creationRequestId", "heroDefinitionId", "characterName"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	port, ok := a.playerData.(CharacterPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	character, err := port.CreateCharacter(r.Context(), session.PlayerID, req)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, character)
}

func (a *api) selectCharacter(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if r.URL.RawQuery != "" {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	var req CharacterSelectionRequest
	if err := DecodeOnlineJSON(r, &req,
		[]string{"selectionRequestId", "characterId", "expectedCharacterRevision"},
		[]string{"selectionRequestId", "characterId", "expectedCharacterRevision"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	port, ok := a.playerData.(CharacterPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	selection, err := port.SelectCharacter(r.Context(), session.PlayerID, req)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, selection)
}

func (a *api) createParty(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if a.party == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	snapshot, err := a.party.CreateParty(r.Context(), session.PlayerID)
	if err != nil {
		writeAPIError(w, http.StatusConflict, "PARTY_CREATE_FAILED", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, snapshot)
}

func (a *api) createMatchmakingTicket(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if a.matchmaking == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
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

// enterDivineBeastsWorld（进入神兽联盟常驻世界）只接受认证主体当前已选中的有效角色。
// 世界实例选择和TransferTicket签发由GameServerControl负责，Gateway不自行选择服务器、不生成票据。
func (a *api) enterDivineBeastsWorld(w http.ResponseWriter, r *http.Request, session AuthenticatedSession) {
	if r.URL.RawQuery != "" || a.worldEntry == nil {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	var req WorldEntryRequest
	if err := DecodeOnlineJSON(
		r,
		&req,
		[]string{"requestId", "characterId", "desiredExperienceId", "expectedCharacterRevision", "preferredRegion"},
		[]string{"requestId", "characterId", "desiredExperienceId", "expectedCharacterRevision"}); err != nil {
		writeOnlineError(w, err)
		return
	}
	if strings.TrimSpace(req.RequestID) == "" || strings.TrimSpace(req.CharacterID) == "" ||
		strings.TrimSpace(req.DesiredExperienceID) == "" || req.ExpectedCharacterRevision < 0 {
		writeOnlineError(w, ServiceError("INVALID_REQUEST"))
		return
	}
	profile, err := a.playerData.GetProfile(r.Context(), session.PlayerID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	if profile.PlayerID != session.PlayerID || profile.GameID == "" ||
		profile.SelectedCharacterID != req.CharacterID {
		writeOnlineError(w, ServiceError("CHARACTER_CONFLICT"))
		return
	}
	characters, ok := a.playerData.(CharacterPort)
	if !ok {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	roster, err := characters.ListCharacters(r.Context(), session.PlayerID)
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	characterValid := false
	for _, character := range roster {
		if character.CharacterID == req.CharacterID &&
			character.CharacterRevision == req.ExpectedCharacterRevision &&
			character.Status == "Active" {
			characterValid = true
			break
		}
	}
	if !characterValid {
		writeOnlineError(w, ServiceError("CHARACTER_CONFLICT"))
		return
	}
	result, err := a.worldEntry.AllocateWorldEntry(r.Context(), WorldEntryAllocationRequest{
		RequestID: req.RequestID, GameID: profile.GameID, PlayerID: session.PlayerID,
		SessionID: session.SessionID, CharacterID: req.CharacterID,
		DesiredExperienceID: req.DesiredExperienceID, PreferredRegion: req.PreferredRegion,
	})
	if err != nil {
		writeOnlineError(w, err)
		return
	}
	if result.CharacterID != req.CharacterID || result.SessionID != session.SessionID ||
		result.ExperienceID != req.DesiredExperienceID || result.TransferTicket == "" ||
		result.Endpoint == "" || result.AssignmentID == "" || result.GameServerID == "" {
		writeOnlineError(w, ServiceError("SERVICE_UNAVAILABLE"))
		return
	}
	w.Header().Set("Cache-Control", "no-store")
	writeJSON(w, http.StatusOK, result)
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
