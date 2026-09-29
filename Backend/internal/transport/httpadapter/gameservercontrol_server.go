package httpadapter

import (
	"context"
	"crypto/subtle"
	"net/http"
	"strings"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// NewGameServerControlHandler（创建游戏服务器控制HTTP处理器）提供内部业务入口；服务器生命周期路由必须持有非空内部Bearer令牌。
func NewGameServerControlHandler(service *gameservercontrol.Service, internalBearerToken string) http.Handler {
	if service == nil {
		panic("GameServerControl Service不能为空")
	}
	internalBearerToken = strings.TrimSpace(internalBearerToken)
	if internalBearerToken == "" {
		panic("GAMESERVERCONTROL_INTERNAL_TOKEN不能为空")
	}
	mux := http.NewServeMux()

	mux.Handle("POST /internal/v1/gameservers/register", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req gameservercontrol.RegisterInput
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if !requireGameServerIdentity(w, r, req.GameServerID, req.ServerBootID) {
			return
		}
		if err := service.Register(req); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_REGISTER_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	}))

	mux.Handle("POST /internal/v1/gameservers/heartbeat", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID   string            `json:"gameServerId"`
			CurrentPlayers int               `json:"currentPlayers"`
			Status         gameserver.Status `json:"status"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, req.GameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(req.GameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		if err := service.Heartbeat(req.GameServerID, req.CurrentPlayers, req.Status); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_HEARTBEAT_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	}))

	mux.Handle("POST /internal/v1/gameservers/ready", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID string `json:"gameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, req.GameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(req.GameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		if err := service.SetReady(req.GameServerID); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_READY_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	}))

	mux.Handle("POST /internal/v1/gameservers/drain", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID string `json:"gameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, req.GameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(req.GameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		if err := service.Drain(req.GameServerID); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_DRAIN_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	}))

	// 以下控制面能力均属于Backend内部边界；即使调用方不是GameServer本身，也必须先通过内部Bearer认证。
	mux.Handle("POST /internal/v1/gameservers/allocate-world", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req gameservercontrol.AllocateWorldInput
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		assignment, err := service.AllocateWorld(r.Context(), req)
		if err != nil {
			writeError(w, http.StatusConflict, "WORLD_ALLOCATION_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, assignment)
	}))

	mux.Handle("POST /internal/v1/gameservers/allocate-world-transfer", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			World              gameservercontrol.AllocateWorldInput `json:"world"`
			TicketID           string                               `json:"ticketId"`
			GameID             string                               `json:"gameId"`
			PlayerID           string                               `json:"playerId"`
			SessionID          string                               `json:"sessionId"`
			SourceGameServerID string                               `json:"sourceGameServerId"`
			TTLMilliseconds    int64                                `json:"ttlMilliseconds"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		ttl := time.Duration(req.TTLMilliseconds) * time.Millisecond
		result, err := service.AllocateWorldTransfer(r.Context(), gameservercontrol.AllocateWorldTransferInput{
			World:    req.World,
			Transfer: gameservercontrol.IssueTransferInput{TicketID: req.TicketID, GameID: req.GameID, PlayerID: req.PlayerID, SessionID: req.SessionID, SourceGameServerID: req.SourceGameServerID, TTL: ttl},
		})
		if err != nil {
			writeError(w, http.StatusConflict, "WORLD_TRANSFER_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	}))

	mux.Handle("POST /internal/v1/gameservers/allocate-mainarena", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req gameservercontrol.AllocateMainArenaInput
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		assignment, err := service.AllocateMainArenaContext(r.Context(), req)
		if err != nil {
			writeError(w, http.StatusConflict, "ARENA_ALLOCATION_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, assignment)
	}))

	mux.Handle("GET /internal/v1/gameservers/assignment", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		gameServerID := strings.TrimSpace(r.URL.Query().Get("gameServerId"))
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, gameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(gameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		assignment, found := service.GetServerAssignment(gameServerID)
		if !found {
			writeError(w, http.StatusNotFound, "GAME_SERVER_ASSIGNMENT_NOT_FOUND", nil)
			return
		}
		writeJSON(w, http.StatusOK, assignment)
	}))

	mux.Handle("POST /internal/v1/gameservers/issue-transfer", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req gameservercontrol.IssueTransferInput
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if req.TTL <= 0 {
			req.TTL = 30 * time.Second
		}
		ticket, err := service.IssueTransfer(req)
		if err != nil {
			writeError(w, http.StatusBadRequest, "TRANSFER_TICKET_ISSUE_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, ticket)
	}))

	mux.Handle("POST /internal/v1/gameservers/validate-transfer", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			Ticket                  servertransfer.Ticket `json:"ticket"`
			DestinationGameServerID string                `json:"destinationGameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		// Ticket验证是目标Dedicated Server动作；内部Bearer之外还必须绑定当前服务器Boot身份。
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, req.DestinationGameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(req.DestinationGameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		result, err := service.ValidateTransferContext(r.Context(), req.Ticket, req.DestinationGameServerID)
		if err != nil {
			writeError(w, http.StatusUnauthorized, "TRANSFER_TICKET_INVALID", err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	}))

	mux.Handle("POST /internal/v1/gameservers/match-result", requireGameServerControlBearer(internalBearerToken, func(w http.ResponseWriter, r *http.Request) {
		var req match.Result
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		serverBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if !requireGameServerIdentity(w, r, req.GameServerID, serverBootID) {
			return
		}
		if err := service.ValidateServerBoot(req.GameServerID, serverBootID); err != nil {
			writeError(w, http.StatusConflict, "GAME_SERVER_BOOT_MISMATCH", err)
			return
		}
		stored, err := service.SubmitMatchResult(r.Context(), req)
		if err != nil {
			writeError(w, http.StatusBadRequest, "MATCH_RESULT_SUBMIT_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, stored)
	}))

	return mux
}

// requireGameServerControlBearer（校验服务器控制Bearer令牌）在解析或执行生命周期请求前拒绝未认证调用，且不回显凭据。
func requireGameServerControlBearer(expectedToken string, next http.HandlerFunc) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		authorization := strings.Fields(r.Header.Get("Authorization"))
		if len(authorization) != 2 || !strings.EqualFold(authorization[0], "Bearer") || subtle.ConstantTimeCompare([]byte(authorization[1]), []byte(expectedToken)) != 1 {
			w.Header().Set("WWW-Authenticate", "Bearer")
			writeError(w, http.StatusUnauthorized, "INTERNAL_AUTHENTICATION_REQUIRED", nil)
			return
		}
		next.ServeHTTP(w, r)
	})
}

// requireGameServerIdentity（校验服务器实例身份头）同时核对GameServerID与ServerBootID。
// BootID不是客户端字段；由Dedicated Server进程启动时生成并随内部控制面请求发送。
func requireGameServerIdentity(
	w http.ResponseWriter,
	r *http.Request,
	requestGameServerID string,
	requestServerBootID string,
) bool {
	headerGameServerID := strings.TrimSpace(r.Header.Get("X-Game-Server-Id"))
	headerServerBootID := strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
	if headerGameServerID == "" || headerGameServerID != requestGameServerID ||
		headerServerBootID == "" || headerServerBootID != requestServerBootID {
		writeError(w, http.StatusForbidden, "GAME_SERVER_IDENTITY_MISMATCH", nil)
		return false
	}
	return true
}

// WorldTransferRequest（世界分配并签发迁移票据的组合请求）供上层业务一次完成目标服务器分配和跨服票据签发。
type WorldTransferRequest struct {
	GameID             string // GameID（游戏ID）。
	PlayerID           string // PlayerID（玩家ID）。
	SessionID          string // SessionID（在线会话ID）。
	SourceGameServerID string // SourceGameServerID（来源服务器）。
	ExperienceID       string // ExperienceID（目标体验）。
	WorldID            string // WorldID（目标世界）。
	RegionID           string // RegionID（目标区域）。
	TicketID           string // TicketID（迁移票据ID）。
}

// AllocateWorldAndIssueTransfer（分配世界并签发迁移票据）实现Hub/Main/Village跨服核心闭环。
func AllocateWorldAndIssueTransfer(ctx context.Context, service *gameservercontrol.Service, req WorldTransferRequest) (gameservercontract.WorldAssignment, servertransfer.Ticket, error) {
	result, err := service.AllocateWorldTransfer(ctx, gameservercontrol.AllocateWorldTransferInput{
		World:    gameservercontrol.AllocateWorldInput{ExperienceID: req.ExperienceID, WorldID: req.WorldID, RegionID: req.RegionID, PlayerSlots: 1},
		Transfer: gameservercontrol.IssueTransferInput{TicketID: req.TicketID, GameID: req.GameID, PlayerID: req.PlayerID, SessionID: req.SessionID, SourceGameServerID: req.SourceGameServerID, TTL: 30 * time.Second},
	})
	if err != nil {
		return gameservercontract.WorldAssignment{}, servertransfer.Ticket{}, err
	}
	return result.Assignment, result.Ticket, nil
}
