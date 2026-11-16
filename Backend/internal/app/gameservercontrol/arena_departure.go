package gameservercontrol

import (
	"context"
	"errors"
)

// ErrWorldMatchMigrationRequired要求当前仍绑定竞技场的玩家向Match取得重连或返程分配。
var ErrWorldMatchMigrationRequired = errors.New("WORLD_MATCH_MIGRATION_REQUIRED")

// authorizeWorldDeparture只根据已接受准入和不可变比赛来源签发返程声明。
func (s *Service) authorizeWorldDeparture(ctx context.Context, playerID, sourceServerID string) (string, error) {
	if s.arenaLifecycle == nil {
		return "", nil
	}
	presence, _, err := s.arenaLifecycle.GetPresence(ctx, playerID)
	if err != nil || presence.MatchID == "" {
		return "", err
	}
	record, found, err := s.arenaLifecycle.GetArenaByMatch(ctx, presence.MatchID)
	if err != nil {
		return "", err
	}
	if !found || record.Assignment.GameServerID != presence.GameServerID || record.ServerBootID != presence.ServerBootID {
		return "", ErrWorldMatchMigrationRequired
	}
	if sourceServerID == presence.GameServerID && (record.State == "resultCommitted" || record.State == "cancelled" || record.State == "released") {
		return presence.MatchID, nil
	}
	if record.State == "released" {
		return "", nil // 历史准入不阻断已回收比赛后的普通登录。
	}
	return "", ErrWorldMatchMigrationRequired
}

// ValidateWorldDeparture在目标服务器接纳前复核同一竞技绑定，旧普通票和其他比赛的返程票都不能覆盖新准入。
// 共享仓储在写入同一玩家位置的条件事务中再次调用；released是不可逆终态。
func ValidateWorldDeparture(previous PlayerPresence, sourceMatchID string, loadArena func(string) (ArenaAssignmentRecord, bool, error)) error {
	if sourceMatchID == previous.MatchID {
		return nil
	}
	if sourceMatchID != "" {
		return ErrWorldMatchMigrationRequired
	}
	record, found, err := loadArena(previous.MatchID)
	if err != nil {
		return err
	}
	if !found || record.State != "released" || record.Assignment.GameServerID != previous.GameServerID || record.ServerBootID != previous.ServerBootID {
		return ErrWorldMatchMigrationRequired
	}
	return nil
}
