package gameservercontrol

import (
	"context"
	"errors"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
)

var ErrArenaStartAbortInvalid = errors.New("ARENA_START_ABORT_INVALID")
var ErrArenaStartAbortConflict = errors.New("ARENA_START_ABORT_CONFLICT")

// ArenaStartAbortInput仅供原MainArena专服报告开战前终止；传输层核对一致的Bearer和服务器身份头。
type ArenaStartAbortInput struct {
	MatchID      string `json:"matchId"`
	GameServerID string `json:"gameServerId"`
	ServerBootID string `json:"serverBootId"`
	Reason       string `json:"reason"`
}

func validArenaStartAbortReason(reason string) bool {
	switch reason {
	case "pre_match_timeout", "player_disconnected", "spawn_failed", "startup_failed":
		return true
	default:
		return false
	}
}

// ReportArenaStartAbort只接受真实分配的MatchId和当前Boot。先关闭持久准入，再排空，原样重试不更改首次事实。
func (s *Service) ReportArenaStartAbort(ctx context.Context, input ArenaStartAbortInput) error {
	if input.MatchID == "" || input.GameServerID == "" || input.ServerBootID == "" || len(input.MatchID) > 128 || len(input.GameServerID) > 128 || len(input.ServerBootID) > 128 || !validArenaStartAbortReason(input.Reason) {
		return ErrArenaStartAbortInvalid
	}
	if s.arenaLifecycle == nil {
		return errors.New("ARENA_STORE_UNAVAILABLE: 未装配持久控制事实")
	}
	s.arenaAllocationMu.Lock()
	defer s.arenaAllocationMu.Unlock()
	instance, registered := s.registry.Get(input.GameServerID)
	if !registered || instance.ServerBootID != input.ServerBootID || instance.RoleID != gameservercontract.RoleMainArena {
		return ErrArenaStartAbortConflict
	}
	record, found, err := s.arenaLifecycle.GetArenaByMatch(ctx, input.MatchID)
	if err != nil {
		return err
	}
	if !found || record.Assignment.GameServerID != input.GameServerID || record.ServerBootID != input.ServerBootID {
		return ErrArenaStartAbortConflict
	}
	if record.State == "active" && instance.CurrentMatchID == input.MatchID {
		if err := s.cancelAssignedArena(ctx, record, input.Reason); err != nil {
			if errors.Is(err, errArenaResultAlreadyCommitted) {
				return ErrArenaStartAbortConflict
			}
			return err
		}
	} else if record.StartAbortReason != input.Reason {
		return ErrArenaStartAbortConflict
	}
	if record.State == "released" {
		return nil
	}
	if err := s.registry.DrainForBoot(input.GameServerID, input.ServerBootID); err != nil {
		return ErrArenaStartAbortConflict
	}
	return nil
}
