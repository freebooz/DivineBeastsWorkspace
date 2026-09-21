// Package match（比赛领域）管理已成立比赛与Dedicated Server提交的权威比赛结果。
package match

import (
	"context"
	"encoding/json"
	"errors"
	"reflect"
	"sync"
	"time"
)

// PlayerResult（玩家比赛结果）由MainArena Dedicated Server权威产生。
type PlayerResult struct {
	PlayerID    string // PlayerID（玩家ID）。
	TeamID      string // TeamID（竞技队伍ID）。
	CharacterID string // CharacterID（本场使用角色ID）。
	Kills       uint32 // Kills（击杀数）。
	Deaths      uint32 // Deaths（死亡数）。
	Assists     uint32 // Assists（助攻数）。
	Score       uint32 // Score（玩家比赛评分）。
}

// TeamResult（队伍比赛结果）由MainArena Dedicated Server权威产生。
type TeamResult struct {
	TeamID string // TeamID（竞技队伍ID）。
	Won    bool   // Won（该队是否获胜）。
	Score  uint32 // Score（队伍比分）。
}

// Result（比赛结果）不接受Game Client直接提交。
type Result struct {
	MatchID       string         // MatchID（比赛ID），同时作为结果提交幂等键。
	ArenaModeID   string         // ArenaModeID（1v1~5v5竞技模式ID）。
	GameServerID  string         // GameServerID（权威MainArena服务器实例ID）。
	StartedAt     time.Time      // StartedAt（比赛开始时间）。
	EndedAt       time.Time      // EndedAt（比赛结束时间）。
	WinningTeamID string         // WinningTeamID（获胜队伍ID，平局时可为空）。
	Teams         []TeamResult   // Teams（双方队伍权威结果）。
	Players       []PlayerResult // Players（全体玩家权威统计）。
}

// StoredResult（已存比赛结果）包含后端生成的稳定ResultID。
type StoredResult struct {
	ResultID string // ResultID（Backend生成的结果记录ID）。
	Result   Result // Result（权威比赛结果内容）。
}

// IntegrationEvent（领域提交产生的跨服务事件）由基础设施层在同一数据库事务中写入Outbox。
type IntegrationEvent struct {
	ID          string    // ID（事件唯一ID/Outbox MessageID）。
	Topic       string    // Topic（事件主题）。
	AggregateID string    // AggregateID（业务聚合ID）。
	Payload     []byte    // Payload（JSON事件正文）。
	OccurredAt  time.Time // OccurredAt（事件发生时间）。
}

// ResultStore（比赛结果仓储接口）隔离PostgreSQL实现。
type ResultStore interface {
	Get(ctx context.Context, matchID string) (StoredResult, bool, error)
	Insert(ctx context.Context, stored StoredResult) error
}

// TransactionalResultStore（事务比赛结果仓储）保证比赛结果与Outbox事件在同一PostgreSQL事务中提交。
type TransactionalResultStore interface {
	ResultStore
	InsertWithEvent(ctx context.Context, stored StoredResult, event IntegrationEvent) error
}

// ResultService（比赛结果服务）提供以MatchID为幂等键的结果提交。
type ResultService struct{ store ResultStore }

// NewResultService（创建比赛结果服务）创建权威结果服务。
func NewResultService(store ResultStore) *ResultService {
	if store == nil {
		panic("Match ResultStore不能为空")
	}
	return &ResultService{store: store}
}

// Submit（提交比赛结果）同一MatchID重复提交完全相同结果时返回同一ResultID；不同结果则拒绝。
// 如果仓储实现TransactionalResultStore，则Match.Completed事件与结果记录保证同事务提交。
func (s *ResultService) Submit(ctx context.Context, result Result) (StoredResult, error) {
	if result.MatchID == "" || result.GameServerID == "" || result.ArenaModeID == "" {
		return StoredResult{}, errors.New("MatchID、GameServerID和ArenaModeID不能为空")
	}
	if current, found, err := s.store.Get(ctx, result.MatchID); err != nil {
		return StoredResult{}, err
	} else if found {
		if !reflect.DeepEqual(current.Result, result) {
			return StoredResult{}, errors.New("MATCH_RESULT_CONFLICT: 同一MatchID出现不同结果")
		}
		return current, nil
	}
	stored := StoredResult{ResultID: "result:" + result.MatchID, Result: cloneResult(result)}
	event, err := matchCompletedEvent(stored)
	if err != nil {
		return StoredResult{}, err
	}
	if transactional, ok := s.store.(TransactionalResultStore); ok {
		if err := transactional.InsertWithEvent(ctx, stored, event); err != nil {
			return StoredResult{}, err
		}
	} else if err := s.store.Insert(ctx, stored); err != nil {
		return StoredResult{}, err
	}
	return stored, nil
}

func matchCompletedEvent(stored StoredResult) (IntegrationEvent, error) {
	payload, err := json.Marshal(struct {
		EventType     string `json:"eventType"`
		EventVersion  int    `json:"eventVersion"`
		MatchID       string `json:"matchId"`
		ArenaModeID   string `json:"arenaModeId"`
		GameServerID  string `json:"gameServerId"`
		WinningTeamID string `json:"winningTeamId,omitempty"`
		EndedAt       string `json:"endedAt"`
	}{EventType: "Match.Completed", EventVersion: 1, MatchID: stored.Result.MatchID, ArenaModeID: stored.Result.ArenaModeID, GameServerID: stored.Result.GameServerID, WinningTeamID: stored.Result.WinningTeamID, EndedAt: stored.Result.EndedAt.UTC().Format(time.RFC3339Nano)})
	if err != nil {
		return IntegrationEvent{}, err
	}
	occurredAt := stored.Result.EndedAt.UTC()
	if occurredAt.IsZero() {
		occurredAt = time.Now().UTC()
	}
	return IntegrationEvent{ID: "match-completed:" + stored.Result.MatchID, Topic: "Match.Completed", AggregateID: stored.Result.MatchID, Payload: payload, OccurredAt: occurredAt}, nil
}

// MemoryResultStore（内存比赛结果仓储）用于单元测试和本地开发。
type MemoryResultStore struct {
	mu     sync.RWMutex
	items  map[string]StoredResult
	events map[string]IntegrationEvent
}

// NewMemoryResultStore（创建内存结果仓储）创建线程安全存储。
func NewMemoryResultStore() *MemoryResultStore {
	return &MemoryResultStore{items: map[string]StoredResult{}, events: map[string]IntegrationEvent{}}
}

// Get（读取比赛结果）按MatchID读取结果。
func (s *MemoryResultStore) Get(_ context.Context, matchID string) (StoredResult, bool, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	item, ok := s.items[matchID]
	item.Result = cloneResult(item.Result)
	return item, ok, nil
}

// Insert（插入比赛结果）只允许首次写入MatchID。
func (s *MemoryResultStore) Insert(_ context.Context, stored StoredResult) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.insertLocked(stored)
}

// InsertWithEvent（事务插入模拟）在同一互斥区内同时写入结果和事件，用于验证事务边界语义。
func (s *MemoryResultStore) InsertWithEvent(_ context.Context, stored StoredResult, event IntegrationEvent) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, exists := s.events[event.ID]; exists {
		return errors.New("Outbox事件已存在")
	}
	if err := s.insertLocked(stored); err != nil {
		return err
	}
	event.Payload = append([]byte(nil), event.Payload...)
	s.events[event.ID] = event
	return nil
}

func (s *MemoryResultStore) insertLocked(stored StoredResult) error {
	if _, exists := s.items[stored.Result.MatchID]; exists {
		return errors.New("比赛结果已存在")
	}
	stored.Result = cloneResult(stored.Result)
	s.items[stored.Result.MatchID] = stored
	return nil
}

// Event（读取内存事务事件）用于单元测试验证Match.Completed与结果同时提交。
func (s *MemoryResultStore) Event(eventID string) (IntegrationEvent, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	value, ok := s.events[eventID]
	value.Payload = append([]byte(nil), value.Payload...)
	return value, ok
}

func cloneResult(result Result) Result {
	result.Teams = append([]TeamResult(nil), result.Teams...)
	result.Players = append([]PlayerResult(nil), result.Players...)
	return result
}
