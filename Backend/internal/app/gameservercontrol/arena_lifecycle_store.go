// 本文件声明竞技分配/安全返回的持久控制事实；结果后保留实例绑定，直到迁出或明确故障终态再回收。
// 只供可信控制面与Match编排使用；公开UI不能读Presence、地址或服务器启动身份。
package gameservercontrol

import (
	"context"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"encoding/json"
	"errors"
	"sync"
	"time"
)

// ArenaAssignmentRecord是已确认竞技分配的恢复记录；同Match不可改变名单/地图，State终态后不得重新激活。
type ArenaAssignmentRecord struct {
	UnallocatedCancellation bool                                // 分配前已撤销的持久墓碑；仅保存MatchID，禁止迟到请求重新占用实例。
	Assignment              gameservercontract.ServerAssignment // 完整权威名单与分配身份。
	Endpoint                string                              // 已注册控制面端点，只供签票。
	ServerBootID            string                              // 原实例真实Boot，重启时拒绝继承。
	InputFingerprint        string                              // 规范化分配意图摘要，防止同比赛异参覆盖。
	State                   string                              // active/resultCommitted/cancelled/released。
	UpdatedAtUTC            time.Time                           // 控制面实际写入时间。
	Result                  *match.StoredResult                 // 已持久可信结果，未完成时为nil；故障取消不能伪造胜负。
}

// PlayerPresence是成功消费准入后的控制事实；不由客户端坐标/Actor名称或队伍UI推断。
type PlayerPresence struct {
	PlayerID      string    // 本人稳定身份。
	SessionID     string    // 当前已认证在线会话。
	GameServerID  string    // 已接受准入的服务器。
	ServerBootID  string    // 原目标Boot，重启立即无效。
	ExperienceID  string    // 当前角色/体验，返回只能分配Village/OpenWorld。
	WorldID       string    // 当前逻辑世界，不是磁盘路径。
	MatchID       string    // 当前比赛，无比赛为空。
	TicketID      string    // 已消费准入身份，不含签名秘密。
	SessionEpoch  uint64    // 同在线会话单调防旧代次。
	IssuedAtUTC   time.Time // 签票时刻，用于跨会话拒绝更早票据覆盖新位置。
	AcceptedAtUTC time.Time // 真实控制面准入接受时间。
}

// ArenaLifecycleStore在生产共享保存分配和本人位置；所有条件检查与索引必须原子提交。
type ArenaLifecycleStore interface {
	GetArenaByMatch(context.Context, string) (ArenaAssignmentRecord, bool, error)
	GetArenaByServer(context.Context, string) (ArenaAssignmentRecord, bool, error)
	SaveArena(context.Context, ArenaAssignmentRecord) error
	GetPresence(context.Context, string) (PlayerPresence, bool, error)
	SavePresence(context.Context, PlayerPresence) error
}

// MemoryArenaLifecycleStore只供明确本地配置/单元测试，不能作为跨进程恢复证据。
type MemoryArenaLifecycleStore struct {
	mu       sync.RWMutex
	matches  map[string]ArenaAssignmentRecord
	servers  map[string]string
	presence map[string]PlayerPresence
}

func NewMemoryArenaLifecycleStore() *MemoryArenaLifecycleStore {
	return &MemoryArenaLifecycleStore{matches: map[string]ArenaAssignmentRecord{}, servers: map[string]string{}, presence: map[string]PlayerPresence{}}
}

// CloneArenaRecord隔离嵌套名单和结果引用；序列化失败传播，不能退化为缺失记录。
func CloneArenaRecord(value ArenaAssignmentRecord) (ArenaAssignmentRecord, error) {
	payload, err := json.Marshal(value)
	if err != nil {
		return ArenaAssignmentRecord{}, err
	}
	var result ArenaAssignmentRecord
	err = json.Unmarshal(payload, &result)
	return result, err
}
func (s *MemoryArenaLifecycleStore) GetArenaByMatch(ctx context.Context, id string) (ArenaAssignmentRecord, bool, error) {
	if err := ctx.Err(); err != nil {
		return ArenaAssignmentRecord{}, false, err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, found := s.matches[id]
	if !found {
		return ArenaAssignmentRecord{}, false, nil
	}
	copy, err := CloneArenaRecord(v)
	return copy, true, err
}
func (s *MemoryArenaLifecycleStore) GetArenaByServer(ctx context.Context, id string) (ArenaAssignmentRecord, bool, error) {
	if err := ctx.Err(); err != nil {
		return ArenaAssignmentRecord{}, false, err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, found := s.matches[s.servers[id]]
	if !found {
		return ArenaAssignmentRecord{}, false, nil
	}
	copy, err := CloneArenaRecord(v)
	return copy, true, err
}

// SaveArena只允许同意图推进或在旧比赛已释放后复用服务器；不删除历史结果，也不能重激活终态。
func (s *MemoryArenaLifecycleStore) SaveArena(ctx context.Context, value ArenaAssignmentRecord) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if err := ValidateArenaRecord(value); err != nil {
		return err
	}
	copy, err := CloneArenaRecord(value)
	if err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	id := value.Assignment.Arena.MatchID
	if old, found := s.matches[id]; found {
		if err := ValidateArenaRecordTransition(old, value); err != nil {
			return err
		}
	}
	if otherID := s.servers[value.Assignment.GameServerID]; value.Assignment.GameServerID != "" && otherID != "" && otherID != id {
		other := s.matches[otherID]
		if other.State != "released" {
			return errors.New("ARENA_SERVER_ALREADY_BOUND: 实例尚未完成上一比赛清理")
		}
	}
	s.matches[id] = copy
	if value.State == "released" {
		if s.servers[value.Assignment.GameServerID] == id {
			delete(s.servers, value.Assignment.GameServerID)
		}
	} else if value.Assignment.GameServerID != "" {
		s.servers[value.Assignment.GameServerID] = id
	}
	return nil
}
func (s *MemoryArenaLifecycleStore) GetPresence(ctx context.Context, id string) (PlayerPresence, bool, error) {
	if err := ctx.Err(); err != nil {
		return PlayerPresence{}, false, err
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, found := s.presence[id]
	return v, found, nil
}

// SavePresence拒绝旧Epoch/更早签票，只有新可信准入能改变本人所在实例；相同接受事实幂等。
func (s *MemoryArenaLifecycleStore) SavePresence(ctx context.Context, value PlayerPresence) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := ValidatePresenceTransition(s.presence[value.PlayerID], value); err != nil {
		return err
	}
	s.presence[value.PlayerID] = value
	return nil
}

// ValidateArenaRecord验证持久事实完整性，不接受空Assignment/Boot或任意未知状态冒充恢复记录。
func ValidateArenaRecord(v ArenaAssignmentRecord) error {
	if v.UnallocatedCancellation {
		if v.Assignment.Arena == nil || v.Assignment.Arena.MatchID == "" || len(v.Assignment.Arena.MatchID) > 128 || v.Assignment.GameServerID != "" || (v.State != "cancelled" && v.State != "released") {
			return errors.New("ARENA_RECORD_INVALID: 未分配取消墓碑无效")
		}
		return nil
	}

	if v.Assignment.Arena == nil || v.ServerBootID == "" || v.Endpoint == "" || v.InputFingerprint == "" {
		return errors.New("ARENA_RECORD_INVALID: 分配恢复上下文缺失")
	}
	if err := v.Assignment.Validate(); err != nil {
		return err
	}
	if v.State != "active" && v.State != "resultCommitted" && v.State != "cancelled" && v.State != "released" {
		return errors.New("ARENA_RECORD_INVALID: 未注册状态")
	}
	if v.State == "resultCommitted" && v.Result == nil {
		return errors.New("ARENA_RECORD_INVALID: 结果阶段缺少可信结果")
	}
	return nil
}

// ValidateArenaRecordTransition保持不可变意图及终态，避免旧工作进程把已经取消/释放的比赛恢复成可入场。
func ValidateArenaRecordTransition(old, next ArenaAssignmentRecord) error {
	if old.UnallocatedCancellation || next.UnallocatedCancellation {
		if old.UnallocatedCancellation && next.UnallocatedCancellation && old.Assignment.Arena.MatchID == next.Assignment.Arena.MatchID && (old.State == "cancelled" || next.State == "released") {
			return nil
		}
		return errors.New("ARENA_OPERATION_TERMINAL: 分配前取消不能被迟到分配覆盖")
	}

	if old.InputFingerprint != next.InputFingerprint || old.Assignment.AssignmentID != next.Assignment.AssignmentID || old.Assignment.GameServerID != next.Assignment.GameServerID || old.ServerBootID != next.ServerBootID {
		return errors.New("ARENA_IDEMPOTENCY_CONFLICT: 分配意图或实例代次改变")
	}
	allowed := map[string]map[string]bool{"active": {"active": true, "resultCommitted": true, "cancelled": true}, "resultCommitted": {"resultCommitted": true, "released": true}, "cancelled": {"cancelled": true, "released": true}, "released": {"released": true}}
	if !allowed[old.State][next.State] {
		return errors.New("ARENA_OPERATION_TERMINAL: 不能倒退或重激活竞技控制事实")
	}
	return nil
}

// ValidatePresenceTransition不能让相同旧票据覆盖新连接；同记录重放只用于受信持久写重试，不放宽票据一次性消费。
func ValidatePresenceTransition(old, next PlayerPresence) error {
	if next.PlayerID == "" || next.SessionID == "" || next.GameServerID == "" || next.ServerBootID == "" || next.SessionEpoch == 0 {
		return errors.New("PLAYER_PRESENCE_INVALID: 准入事实不完整")
	}
	if old.TicketID == next.TicketID && old.SessionID == next.SessionID && old.SessionEpoch == next.SessionEpoch {
		return nil
	}
	if (old.SessionID == next.SessionID && next.SessionEpoch <= old.SessionEpoch) || next.IssuedAtUTC.Before(old.IssuedAtUTC) {
		return errors.New("PLAYER_PRESENCE_STALE: 旧准入不能覆盖新位置")
	}
	return nil
}
