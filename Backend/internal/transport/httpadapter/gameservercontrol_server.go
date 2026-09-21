package httpadapter

import (
	"context"
	"net/http"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// NewGameServerControlHandler（创建游戏服务器控制HTTP处理器）提供本地联调及gRPC不可用环境下的真实业务传输入口。
func NewGameServerControlHandler(service *gameservercontrol.Service) http.Handler {
	if service == nil {
		panic("GameServerControl Service不能为空")
	}
	mux := http.NewServeMux()

	mux.HandleFunc("POST /internal/v1/gameservers/register", func(w http.ResponseWriter, r *http.Request) {
		var req gameservercontrol.RegisterInput
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if err := service.Register(req); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_REGISTER_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	})

	mux.HandleFunc("POST /internal/v1/gameservers/heartbeat", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID   string            `json:"gameServerId"`
			CurrentPlayers int               `json:"currentPlayers"`
			Status         gameserver.Status `json:"status"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if err := service.Heartbeat(req.GameServerID, req.CurrentPlayers, req.Status); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_HEARTBEAT_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	})

	mux.HandleFunc("POST /internal/v1/gameservers/ready", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID string `json:"gameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if err := service.SetReady(req.GameServerID); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_READY_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	})

	mux.HandleFunc("POST /internal/v1/gameservers/drain", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			GameServerID string `json:"gameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		if err := service.Drain(req.GameServerID); err != nil {
			writeError(w, http.StatusBadRequest, "GAME_SERVER_DRAIN_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"accepted": true})
	})

	mux.HandleFunc("POST /internal/v1/gameservers/allocate-world", func(w http.ResponseWriter, r *http.Request) {
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
	})

	mux.HandleFunc("POST /internal/v1/gameservers/allocate-world-transfer", func(w http.ResponseWriter, r *http.Request) {
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
	})

	mux.HandleFunc("POST /internal/v1/gameservers/allocate-mainarena", func(w http.ResponseWriter, r *http.Request) {
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
	})

	mux.HandleFunc("GET /internal/v1/gameservers/assignment", func(w http.ResponseWriter, r *http.Request) {
		assignment, found := service.GetServerAssignment(r.URL.Query().Get("gameServerId"))
		if !found {
			writeError(w, http.StatusNotFound, "GAME_SERVER_ASSIGNMENT_NOT_FOUND", nil)
			return
		}
		writeJSON(w, http.StatusOK, assignment)
	})

	mux.HandleFunc("POST /internal/v1/gameservers/issue-transfer", func(w http.ResponseWriter, r *http.Request) {
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
	})

	mux.HandleFunc("POST /internal/v1/gameservers/validate-transfer", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			Ticket                  servertransfer.Ticket `json:"ticket"`
			DestinationGameServerID string                `json:"destinationGameServerId"`
		}
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		result, err := service.ValidateTransferContext(r.Context(), req.Ticket, req.DestinationGameServerID)
		if err != nil {
			writeError(w, http.StatusUnauthorized, "TRANSFER_TICKET_INVALID", err)
			return
		}
		writeJSON(w, http.StatusOK, result)
	})

	mux.HandleFunc("POST /internal/v1/gameservers/match-result", func(w http.ResponseWriter, r *http.Request) {
		var req match.Result
		if err := decodeJSON(r, &req); err != nil {
			writeError(w, http.StatusBadRequest, "BAD_REQUEST", err)
			return
		}
		stored, err := service.SubmitMatchResult(r.Context(), req)
		if err != nil {
			writeError(w, http.StatusBadRequest, "MATCH_RESULT_SUBMIT_FAILED", err)
			return
		}
		writeJSON(w, http.StatusOK, stored)
	})

	return mux
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
