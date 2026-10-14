// Package matchapi（MatchService业务API）提供Party创建和Matchmaking Ticket创建的稳定应用层入口。
package matchapi

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sort"
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
	parties PartyRepository
	tickets TicketRepository
	nextID  func(prefix string) string
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

// CreateMatchmakingTicket（创建匹配票据）按ClientRequestID保证幂等，并在Party进入Queue前锁定Roster。
func (s *Service) CreateMatchmakingTicket(ctx context.Context, actorPlayerID string, input CreateMatchmakingTicketInput) (matchmaking.Ticket, error) {
	if actorPlayerID == "" || input.ClientRequestID == "" || input.RegionID == "" {
		return matchmaking.Ticket{}, errors.New("PlayerID、ClientRequestID和RegionID不能为空")
	}
	// 请求ID只在主体域内唯一；摘要分开存储，异参重试明确冲突，而不是创建另一票据或返回他人资料。
	requestKey := scopedOperationKey(actorPlayerID, "matchmaking", input.ClientRequestID)
	fingerprint := fingerprintOf(input)
	if current, found, err := s.tickets.GetByClientRequestID(ctx, requestKey); err != nil {
		return matchmaking.Ticket{}, err
	} else if found {
		if current.RequestFingerprint != fingerprint {
			return matchmaking.Ticket{}, errors.New("MATCH_IDEMPOTENCY_CONFLICT: 同一请求标识对应不同参数")
		}
		return current, nil
	}

	partyID := input.PartyID
	memberIDs := []string{actorPlayerID}
	var currentParty *party.Party
	var originalPartyRevision int64
	if partyID != "" {
		value, err := s.parties.Get(ctx, partyID)
		if err != nil {
			return matchmaking.Ticket{}, err
		}
		if value.LeaderPlayerID != actorPlayerID {
			return matchmaking.Ticket{}, errors.New("PARTY_NOT_LEADER: 只有队长可以开始匹配")
		}
		if value.RosterLocked {
			return matchmaking.Ticket{}, errors.New("PARTY_ROSTER_LOCKED: Party已经进入匹配")
		}
		originalPartyRevision = value.Revision
		if err := value.SelectArenaMode(actorPlayerID, input.ArenaModeID); err != nil {
			return matchmaking.Ticket{}, err
		}
		value.LockRoster()
		currentParty = value
		memberIDs = make([]string, 0, len(value.Members))
		for playerID := range value.Members {
			memberIDs = append(memberIDs, playerID)
		}
		sort.Strings(memberIDs)
	}

	ticket, err := matchmaking.NewTicket(matchmaking.CreateTicketRequest{
		TicketID:       s.nextID("matchmaking"),
		GameID:         "divine-beasts",
		ArenaModeID:    input.ArenaModeID,
		PartyID:        partyID,
		PartyMemberIDs: memberIDs,
		Region:         input.RegionID,
	})
	if err != nil {
		if currentParty != nil {
			currentParty.UnlockRoster()
		}
		return matchmaking.Ticket{}, err
	}
	if currentParty != nil {
		atomic, ok := s.parties.(AtomicPartyRepository)
		if !ok {
			return matchmaking.Ticket{}, errors.New("PARTY_ATOMIC_STORE_REQUIRED: 匹配必须使用条件事务仓储")
		}
		if _, err := atomic.Update(ctx, currentParty.ID, originalPartyRevision, "", "", func(value *party.Party) error { *value = *currentParty.Clone(); return nil }); err != nil {
			return matchmaking.Ticket{}, err
		}
	}
	ticket.RequestFingerprint = fingerprint
	if err := s.tickets.Save(ctx, requestKey, ticket); err != nil {
		if currentParty != nil {
			if atomic, ok := s.parties.(AtomicPartyRepository); ok {
				if _, rollbackErr := atomic.Update(ctx, currentParty.ID, currentParty.Revision, "", "", func(value *party.Party) error { value.UnlockRoster(); return nil }); rollbackErr != nil {
					return matchmaking.Ticket{}, errors.Join(err, fmtPartyRollback(rollbackErr))
				}
			}
		}
		return matchmaking.Ticket{}, err
	}
	return ticket, nil
}

// MemoryPartyRepository（内存Party仓储）用于单元测试与本地联调。
type MemoryPartyRepository struct {
	mu       sync.RWMutex
	items    map[string]*party.Party
	players  map[string]string
	receipts map[string]PartyOperationReceipt
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
		if owner := r.players[id]; owner != "" && owner != value.ID {
			return errors.New("PARTY_ALREADY_MEMBER: 玩家已在其他队伍")
		}
	}
	r.items[value.ID] = cloneParty(value)
	for id := range value.Members {
		r.players[id] = value.ID
	}
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
