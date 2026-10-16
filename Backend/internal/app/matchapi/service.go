// Package matchapi（MatchService业务API）提供Party创建和Matchmaking Ticket创建的稳定应用层入口。
package matchapi

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sync"

	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
)

// PartyRepository（Party仓储接口）隔离Redis等实时Party状态存储实现。
type PartyRepository interface {
	Get(ctx context.Context, partyID string) (*party.Party, error)
	Save(ctx context.Context, value *party.Party) error
}

// TicketRepository（匹配票据仓储接口）隔离Redis队列与幂等索引实现。
type TicketRepository interface {
	GetByClientRequestID(ctx context.Context, clientRequestID string) (matchmaking.Ticket, bool, error)
	Save(ctx context.Context, clientRequestID string, ticket matchmaking.Ticket) error
}

// CreateMatchmakingTicketInput（创建匹配票据输入）只包含Game Client有权声明的业务字段。
type CreateMatchmakingTicketInput struct {
	ArenaModeID     string // ArenaModeID（1v1至5v5竞技模式ID）。
	PartyID         string // PartyID（组队ID，单排可为空）。
	RegionID        string // RegionID（首选匹配区域）。
	ClientRequestID string // ClientRequestID（客户端幂等请求ID）。
}

// Service（Match业务API服务）组合Party和Matchmaking仓储并落实名单锁定与请求幂等规则。
type Service struct {
	parties             PartyRepository
	tickets             TicketRepository
	nextID              func(prefix string) string
	observerMu          sync.Mutex // 仅管理本实例在途观察；业务数据与跨进程通知仍归仓储。
	observers           map[string]*partyObserver
	matchmakingSettings *MatchmakingSettings // 组合根启动前设置；nil仅保留旧测试装配，不启用新生命周期命令。
	matchmakingRuntime  MatchmakingRuntime   // 真实持久步骤/本人迁移提供者；缺失时绝不发布假准入。
}

// NewService（创建Match业务API服务）注入Party仓储、Ticket仓储和ID生成器。
func NewService(parties PartyRepository, tickets TicketRepository, nextID func(prefix string) string) *Service {
	if parties == nil || tickets == nil || nextID == nil {
		panic("Match API Service依赖不能为空")
	}
	return &Service{parties: parties, tickets: tickets, nextID: nextID}
}

// CreateParty（创建Party）以当前玩家作为队长和首个在线成员。
func (s *Service) CreateParty(ctx context.Context, playerID, displayName string) (*party.Party, error) {
	if playerID == "" {
		return nil, errors.New("PlayerID不能为空")
	}
	if atomic, ok := s.parties.(AtomicPartyRepository); ok {
		if current, err := atomic.GetByPlayerID(ctx, playerID); err == nil {
			return current, nil
		} else if !errors.Is(err, ErrPartyNotFound) {
			return nil, err
		}
	}
	value := party.New(s.nextID("party"), party.Member{PlayerID: playerID, DisplayName: displayName, Online: true})
	if err := s.parties.Save(ctx, value); err != nil {
		return nil, err
	}
	return cloneParty(value), nil
}

// CreateMatchmakingTicket在同一仓储事务提交名单锁、票据、唯一玩家预留与主体幂等回执。
// 单人无需建立队伍；已经属于小队的玩家不能用空PartyID绕开原子名单。
func (s *Service) CreateMatchmakingTicket(ctx context.Context, actorPlayerID string, input CreateMatchmakingTicketInput) (matchmaking.Ticket, error) {
	if actorPlayerID == "" || input.ClientRequestID == "" || input.RegionID == "" || len(actorPlayerID) > 128 || len(input.ClientRequestID) > 128 || len(input.RegionID) > 128 || len(input.PartyID) > 128 {
		return matchmaking.Ticket{}, errors.New("MATCH_COMMAND_INVALID: 报名主体、请求或区域无效")
	}
	return s.enqueueAtomically(ctx, actorPlayerID, input)
}

// MemoryPartyRepository（内存Party仓储）用于单元测试与本地联调。
type MemoryPartyRepository struct {
	mu       sync.RWMutex
	items    map[string]*party.Party
	players  map[string]string
	receipts map[string]PartyOperationReceipt
	changes  map[string]*partyMemoryChanges // 测试/本地事件日志，不替代生产Redis Stream。
	queue    *matchmaking.QueueState        // 与Party、成员索引使用同一事务锁；只用于明确本地/测试仓储。
}

// NewMemoryPartyRepository（创建内存Party仓储）创建线程安全Party存储。
func NewMemoryPartyRepository() *MemoryPartyRepository {
	return &MemoryPartyRepository{items: map[string]*party.Party{}, players: map[string]string{}, receipts: map[string]PartyOperationReceipt{}}
}

// Get（读取Party）返回独立副本，避免调用方绕过Save直接修改仓储状态。
func (r *MemoryPartyRepository) Get(_ context.Context, partyID string) (*party.Party, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	value, found := r.items[partyID]
	if !found {
		return nil, ErrPartyNotFound
	}
	return cloneParty(value), nil
}

// Save（保存Party）写入独立副本。
func (r *MemoryPartyRepository) Save(_ context.Context, value *party.Party) error {
	if value == nil || value.ID == "" {
		return errors.New("Party不能为空")
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if _, exists := r.items[value.ID]; exists {
		return errors.New("PARTY_CONDITIONAL_UPDATE_REQUIRED: 已有队伍必须条件更新")
	}
	for id := range value.Members {
		if r.queue != nil && r.queue.Reservations[id] != "" {
			return errors.New("MATCH_PLAYER_RESERVED: 排队或比赛期间不能创建其他队伍")
		}
		if owner := r.players[id]; owner != "" && owner != value.ID {
			return errors.New("PARTY_ALREADY_MEMBER: 玩家已在其他队伍")
		}
	}
	r.items[value.ID] = cloneParty(value)
	for id := range value.Members {
		r.players[id] = value.ID
	}
	r.publishPartyChangesLocked(nil, value)
	return nil
}

// MemoryTicketRepository（内存匹配票据仓储）按ClientRequestID提供幂等查询。
type MemoryTicketRepository struct {
	mu        sync.RWMutex
	byRequest map[string]matchmaking.Ticket
}

// NewMemoryTicketRepository（创建内存Ticket仓储）创建线程安全幂等索引。
func NewMemoryTicketRepository() *MemoryTicketRepository {
	return &MemoryTicketRepository{byRequest: map[string]matchmaking.Ticket{}}
}

// GetByClientRequestID（按客户端请求ID查询）返回已有Ticket以支持安全重试。
func (r *MemoryTicketRepository) GetByClientRequestID(_ context.Context, clientRequestID string) (matchmaking.Ticket, bool, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	value, found := r.byRequest[clientRequestID]
	value.PartyMemberIDs = append([]string(nil), value.PartyMemberIDs...)
	return value, found, nil
}

// Save（保存匹配票据）同一ClientRequestID只保存首次结果。
func (r *MemoryTicketRepository) Save(_ context.Context, clientRequestID string, ticket matchmaking.Ticket) error {
	if clientRequestID == "" || ticket.TicketID == "" {
		return errors.New("ClientRequestID和TicketID不能为空")
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if current, found := r.byRequest[clientRequestID]; found {
		if current.TicketID != ticket.TicketID {
			return errors.New("MATCH_IDEMPOTENCY_CONFLICT: ClientRequestID已对应其他Ticket")
		}
		return nil
	}
	ticket.PartyMemberIDs = append([]string(nil), ticket.PartyMemberIDs...)
	r.byRequest[clientRequestID] = ticket
	return nil
}

func cloneParty(value *party.Party) *party.Party {
	return value.Clone()
}

// scopedOperationKey（主体域幂等键）使用长度无歧义JSON摘要，避免分隔符碰撞和跨主体读取。
func scopedOperationKey(playerID, operation, requestID string) string {
	return fingerprintOf([]string{playerID, operation, requestID})
}

// fingerprintOf（标准化内容摘要）输入均为固定字段结构，不接收用户提供的任意map或敏感认证材料。
func fingerprintOf(value any) string {
	payload, err := json.Marshal(value)
	if err != nil {
		panic("固定组队契约序列化失败")
	}
	digest := sha256.Sum256(payload)
	return hex.EncodeToString(digest[:])
}

// fmtPartyRollback（保留补偿失败）避免只返回票据错误而隐藏仍待解锁的业务状态。
func fmtPartyRollback(err error) error { return errors.New("PARTY_ROLLBACK_PENDING: " + err.Error()) }
