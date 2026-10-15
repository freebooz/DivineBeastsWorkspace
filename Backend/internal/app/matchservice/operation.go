// 本文件声明匹配跨服务持久步骤与栅栏租约；签票秘密只供后端恢复/本人迁移，不能进入事件、UI快照或日志。
package matchservice

import (
	"context"
	"crypto/sha256"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sync"
	"time"
)

var ErrOperationBusy = errors.New("MATCH_OPERATION_BUSY: 其他工作者仍持有租约")
var ErrOperationLeaseLost = errors.New("MATCH_OPERATION_LEASE_LOST: 工作步骤租约或版本已失效")

// ReturnIntent保存本人安全返回的稳定身份与短期准入，只由已认证本人取用；同键异参不能覆盖。
type ReturnIntent struct {
	ClientRequestID string                // 本人逻辑迁移身份。
	SessionID       string                // 签票时已认证当前会话。
	CharacterID     string                // 本局锁定且后端复核的角色。
	TicketID        string                // 可在响应未知时重试/撤销的稳定迁移身份。
	Migration       matchmaking.Migration // 短期秘密，只传给本人Session。
	Completed       bool                  // 后端观察真实目标准入后确认，不能由UI自行声明。
}

// Operation是可恢复的比赛编排记录，Version/Fence由仓储授权，旧工作进程不得覆盖后继步骤。
type Operation struct {
	Reconnects          map[string]ReturnIntent             // 每人当前恢复准入意图；只换连接票据，角色/队伍/出生和60秒资格仍由原服务器裁定。
	ID                  string                              // 与候选一一对应。
	MatchID             string                              // 分配/结果幂等身份，跨重试不变。
	InputFingerprint    string                              // 不可变名单、模式、区域、角色/来源与内容规则摘要。
	Candidate           matchmaking.Candidate               // 已全员接受的不可变名单；状态变化不重新分队。
	Parties             map[string]string                   // 已确认玩家对应不可拆分原队伍，单排为空。
	Contexts            map[string]matchmaking.ActorContext // 逐人已验证的原会话/锁定角色及来源体验。
	MapID               string                              // 真实登记地图逻辑身份。
	RuleRevision        int32                               // 批准规则修订，和UE相同精确整数。
	ContentRevision     string                              // 真实内容修订。
	HeroCatalogRevision int32                               // 已发布英雄目录版本。
	Stage               string                              // allocating/signing/committed/inMatch/returning/rollingBack/cancelled/completed。
	Assignment          *gameservercontract.Assignment      // 控制面真实分配，未完成为nil。
	TicketIDs           map[string]string                   // 每人稳定签票意图，发请求前先持久化。
	Tickets             map[string]servertransfer.Ticket    // 部分签票回执；全员完成前不向任何玩家发布。
	Returns             map[string]ReturnIntent             // 本人返回步骤，不广播秘密。
	Result              *match.StoredResult                 // 可信结果；故障取消不能构造正常胜负。
	FaultCode           string                              // 稳定故障原因，不能保存底层异常原文。
	DeadlineUTC         time.Time                           // 分配/入场当前权威期限。
	CreatedAtUTC        time.Time                           // 实际意图创建时刻。
	UpdatedAtUTC        time.Time                           // 最近已提交步骤时刻。
	Version             int64                               // 单调持久版本，Save必须与读取版本一致。
	Fence               int64                               // 每次领取递增的工作者栅栏代次。
	LeaseOwner          string                              // 本次独立处理请求身份，不是进程全局玩家/比赛单例。
	LeaseUntilUTC       time.Time                           // 租约期限；外部请求必须受此截止时间约束。
}

// CloneOperation深复制所有含秘密/名单的嵌套值，仅在后端内存使用，不能打印序列化正文。
func CloneOperation(value Operation) (Operation, error) {
	payload, err := json.Marshal(value)
	if err != nil {
		return Operation{}, err
	}
	var result Operation
	err = json.Unmarshal(payload, &result)
	return result, err
}

// OperationInputFingerprint只摘要固定不可变输入，不把临时租约、回执或票据秘密计入幂等内容。
func OperationInputFingerprint(value Operation) (string, error) {
	input := struct {
		ID, MatchID, MapID, ModeID, RegionID, ContentRevision string
		RuleRevision, HeroCatalogRevision                     int32
		Roster                                                []matchmaking.CandidateSlot
		Contexts                                              map[string]matchmaking.ActorContext
		Parties, TicketIDs                                    map[string]string
	}{value.ID, value.MatchID, value.MapID, value.Candidate.ArenaModeID, value.Candidate.RegionID, value.ContentRevision, value.RuleRevision, value.HeroCatalogRevision, value.Candidate.Roster, value.Contexts, value.Parties, value.TicketIDs}
	payload, err := json.Marshal(input)
	if err != nil {
		return "", err
	}
	hash := sha256.Sum256(payload)
	return hex.EncodeToString(hash[:]), nil
}

// ValidateOperationInput拒绝未确认名单、缺少真实内容或角色上下文的分配意图；不能把空对象保存为成功步骤。
func ValidateOperationInput(value Operation) error {
	mode, known := matchmaking.ArenaModeByID(value.Candidate.ArenaModeID)
	if err := ValidateOperationTransition(value.Stage, value.Stage); err != nil {
		return err
	}
	if value.ID == "" || value.MatchID == "" || len(value.ID) > 128 || value.MapID == "" || value.RuleRevision < 1 || value.HeroCatalogRevision < 1 || value.ContentRevision == "" || !known || len(value.Candidate.Roster) != mode.TotalPlayers || value.TicketIDs == nil || value.Tickets == nil || value.Returns == nil {
		return errors.New("MATCH_OPERATION_INVALID: 意图/名单/内容不完整")
	}
	seen := map[string]bool{}
	teams := map[string]int{}
	for _, slot := range value.Candidate.Roster {
		actor, found := value.Contexts[slot.PlayerID]
		if !slot.Accepted || seen[slot.PlayerID] || !found || actor.PlayerID != slot.PlayerID || actor.SessionID == "" || actor.CharacterID == "" || actor.HeroDefinitionID == "" || actor.CharacterRevision < 1 || value.TicketIDs[slot.PlayerID] == "" {
			return errors.New("MATCH_OPERATION_INVALID: 缺少逐人确认、角色或签票身份")
		}
		seen[slot.PlayerID] = true
		teams[slot.TeamID]++
	}
	if len(teams) != 2 {
		return errors.New("MATCH_OPERATION_INVALID: 必须恰好两队")
	}
	for _, count := range teams {
		if count != mode.TeamSize {
			return errors.New("MATCH_OPERATION_INVALID: 队伍人数无效")
		}
	}
	fingerprint, err := OperationInputFingerprint(value)
	if err != nil {
		return err
	}
	if fingerprint != value.InputFingerprint {
		return errors.New("MATCH_OPERATION_CONFLICT: 不可变输入摘要不一致")
	}
	return nil
}

// OperationStore由PostgreSQL持久步骤与事务Outbox实现；领取有期限，Save/Release必须核对Owner、Fence及Version。
type OperationStore interface {
	GetOperation(context.Context, string) (Operation, bool, error)
	ClaimOperation(context.Context, Operation, string, time.Time, time.Duration) (Operation, error)
	SaveOperation(context.Context, Operation, time.Time, time.Duration) (Operation, error)
	ReleaseOperation(context.Context, string, string, int64) error
}

// MemoryOperationStore仅为单进程开发/确定性回归，不替代生产PostgreSQL与发件箱。
type MemoryOperationStore struct {
	mu    sync.Mutex
	items map[string]Operation
}

func NewMemoryOperationStore() *MemoryOperationStore {
	return &MemoryOperationStore{items: map[string]Operation{}}
}
func (s *MemoryOperationStore) GetOperation(ctx context.Context, id string) (Operation, bool, error) {
	if err := ctx.Err(); err != nil {
		return Operation{}, false, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	value, found := s.items[id]
	if !found {
		return Operation{}, false, nil
	}
	copy, err := CloneOperation(value)
	return copy, true, err
}

// ClaimOperation只在首次保存合法意图，已有意图必须完全相同；租约有效的另一拥有者使本次领取失败。
func (s *MemoryOperationStore) ClaimOperation(ctx context.Context, proposal Operation, owner string, now time.Time, lease time.Duration) (Operation, error) {
	if err := ctx.Err(); err != nil {
		return Operation{}, err
	}
	if owner == "" || lease <= 0 || lease > 30*time.Second {
		return Operation{}, errors.New("MATCH_OPERATION_INVALID: 租约参数无效")
	}
	if err := ValidateOperationInput(proposal); err != nil {
		return Operation{}, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	current, found := s.items[proposal.ID]
	if found {
		if current.InputFingerprint != proposal.InputFingerprint {
			return Operation{}, errors.New("MATCH_OPERATION_CONFLICT: 同身份对应不同意图")
		}
		if current.LeaseOwner != "" && current.LeaseOwner != owner && current.LeaseUntilUTC.After(now) {
			return Operation{}, ErrOperationBusy
		}
	} else {
		var err error
		current, err = CloneOperation(proposal)
		if err != nil {
			return Operation{}, err
		}
		current.Version = 0
	}
	if current.Fence == int64(^uint64(0)>>1) || current.Version == int64(^uint64(0)>>1) {
		return Operation{}, errors.New("MATCH_OPERATION_VERSION_EXHAUSTED: 栅栏或版本耗尽")
	}
	current.Fence++
	current.Version++
	current.LeaseOwner = owner
	current.LeaseUntilUTC = now.Add(lease)
	current.UpdatedAtUTC = now
	copy, err := CloneOperation(current)
	if err != nil {
		return Operation{}, err
	}
	s.items[current.ID] = copy
	return current, nil
}

// SaveOperation拒绝旧租约/版本及不可变输入变化；只在全部检查成功后保存步骤，不允许部分回执泄漏。
func (s *MemoryOperationStore) SaveOperation(ctx context.Context, next Operation, now time.Time, lease time.Duration) (Operation, error) {
	if err := ctx.Err(); err != nil {
		return Operation{}, err
	}
	if lease <= 0 || lease > 30*time.Second {
		return Operation{}, errors.New("MATCH_OPERATION_INVALID: 租约参数无效")
	}
	if err := ValidateOperationInput(next); err != nil {
		return Operation{}, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	old, found := s.items[next.ID]
	if !found || old.Version != next.Version || old.Fence != next.Fence || old.LeaseOwner != next.LeaseOwner || !old.LeaseUntilUTC.After(now) {
		return Operation{}, ErrOperationLeaseLost
	}
	if old.InputFingerprint != next.InputFingerprint {
		return Operation{}, errors.New("MATCH_OPERATION_CONFLICT: 步骤不能改变不可变输入")
	}
	if err := ValidateOperationTransition(old.Stage, next.Stage); err != nil {
		return Operation{}, err
	}
	if next.Version >= int64(^uint64(0)>>1) {
		return Operation{}, errors.New("MATCH_OPERATION_VERSION_EXHAUSTED: 版本耗尽")
	}
	next.Version++
	next.UpdatedAtUTC = now
	next.LeaseUntilUTC = now.Add(lease)
	copy, err := CloneOperation(next)
	if err != nil {
		return Operation{}, err
	}
	s.items[next.ID] = copy
	return next, nil
}

// ReleaseOperation只解除精确租约；旧工作者不能撤销已被后继领取的新栅栏。
func (s *MemoryOperationStore) ReleaseOperation(ctx context.Context, id, owner string, fence int64) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	value, found := s.items[id]
	if !found || value.Fence != fence || value.LeaseOwner != owner {
		return ErrOperationLeaseLost
	}
	value.LeaseOwner = ""
	value.LeaseUntilUTC = time.Time{}
	s.items[id] = value
	return nil
}

// ValidateOperationTransition只允许前进/补偿或同阶段回执保存；终态不能被迟到工作者重激活。
func ValidateOperationTransition(old, next string) error {
	allowed := map[string]map[string]bool{"allocating": {"allocating": true, "signing": true, "rollingBack": true}, "signing": {"signing": true, "committed": true, "rollingBack": true}, "committed": {"committed": true, "inMatch": true, "returning": true, "rollingBack": true}, "inMatch": {"inMatch": true, "returning": true, "rollingBack": true}, "returning": {"returning": true, "completed": true}, "rollingBack": {"rollingBack": true, "returning": true, "cancelled": true}, "cancelled": {"cancelled": true}, "completed": {"completed": true}}
	if !allowed[old][next] {
		return errors.New("MATCH_OPERATION_PHASE_CONFLICT: 持久步骤不能倒退或重激活")
	}
	return nil
}
