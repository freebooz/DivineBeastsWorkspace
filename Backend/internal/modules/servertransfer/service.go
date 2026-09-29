// Package servertransfer（服务器迁移领域）负责签发和验证短期、一次性、目标绑定的迁移票据。
package servertransfer

import (
	"context"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"sync"
	"time"
)

// IssueRequest（迁移票据签发请求）描述玩家从当前服务器迁移到目标服务器所需上下文。
type IssueRequest struct {
	TicketID                   string        // TicketID（迁移票据ID）。
	AssignmentID               string        // AssignmentID（目标世界或比赛分配ID）。
	GameID                     string        // GameID（游戏ID）。
	PlayerID                   string        // PlayerID（玩家ID）。
	SessionID                  string        // SessionID（在线会话ID）。
	SourceGameServerID         string        // SourceGameServerID（来源服务器实例ID，首次进服可为空）。
	DestinationGameServerID    string        // DestinationGameServerID（目标服务器实例ID）。
	DestinationEndpoint        string        // DestinationEndpoint（ClientTravel目标地址）。
	DestinationWorldID         string        // DestinationWorldID（目标世界ID）。
	DestinationExperienceID    string        // DestinationExperienceID（目标体验，例如Lobby.Main）。
	MatchID                    string        // MatchID（进入MainArena时的比赛ID；普通世界迁移为空）。
	TTL                        time.Duration // TTL（票据有效时长）。
	DestinationServerBootID    string        // DestinationServerBootID（目标服务器启动代次，服务器重启后必须变化）。
	DestinationProtocolVersion uint32        // DestinationProtocolVersion（目标UE实时网络协议版本）。

}

// Ticket（迁移票据）是Game Client ClientTravel前获取的短期凭据。
type Ticket struct {
	TicketID                   string `json:"ticketId"`                     // TicketID（迁移票据唯一ID）。
	AssignmentID               string `json:"assignmentId"`                 // AssignmentID（目标Assignment ID）。
	GameID                     string `json:"gameId"`                       // GameID（游戏ID）。
	PlayerID                   string `json:"playerId"`                     // PlayerID（玩家ID）。
	SessionID                  string `json:"sessionId"`                    // SessionID（在线会话ID）。
	SourceGameServerID         string `json:"sourceGameServerId,omitempty"` // SourceGameServerID（来源服务器实例ID）。
	DestinationGameServerID    string `json:"destinationGameServerId"`      // DestinationGameServerID（目标服务器实例ID）。
	DestinationEndpoint        string `json:"destinationEndpoint"`          // DestinationEndpoint（目标连接地址）。
	DestinationWorldID         string `json:"destinationWorldId"`           // DestinationWorldID（目标世界ID）。
	DestinationExperienceID    string `json:"destinationExperienceId"`      // DestinationExperienceID（目标Experience）。
	MatchID                    string `json:"matchId,omitempty"`            // MatchID（比赛ID，普通世界迁移可为空）。
	GameSessionID              string `json:"gameSessionId"`                // GameSessionID（本次权威游戏会话绑定ID）。
	DestinationServerBootID    string `json:"destinationServerBootId"`      // DestinationServerBootID（目标服务器启动代次）。
	DestinationProtocolVersion uint32 `json:"destinationProtocolVersion"`   // DestinationProtocolVersion（目标实时网络协议版本）。
	SessionEpoch               uint64 `json:"sessionEpoch"`                 // SessionEpoch（在线Session单调递增绑定代次）。

	IssuedAt  time.Time `json:"issuedAt"`  // IssuedAt（签发时间）。
	ExpiresAt time.Time `json:"expiresAt"` // ExpiresAt（过期时间）。
	Nonce     string    `json:"nonce"`     // Nonce（一次性随机值）。
	Signature string    `json:"signature"` // Signature（HMAC-SHA256签名）。
}

// ValidateRequest（迁移票据验证请求）由目标Dedicated Server提交。
type ValidateRequest struct {
	Ticket                     Ticket // Ticket（待验证迁移票据）。
	DestinationGameServerID    string // DestinationGameServerID（正在执行验证的目标服务器ID）。
	DestinationServerBootID    string // DestinationServerBootID（当前目标进程Boot身份）。
	DestinationProtocolVersion uint32 // DestinationProtocolVersion（当前目标进程网络协议版本）。
}

// DestinationServerBootID与DestinationProtocolVersion来自目标Dedicated Server自身注册身份，
// 不能从客户端请求体或票据外部的非可信字段推断。

// ValidationResult（迁移验证结果）返回目标服务器建立玩家会话所需的可信上下文。
type ValidationResult struct {
	PlayerID                   string `json:"playerId"`                   // PlayerID（已验证玩家ID）。
	SessionID                  string `json:"sessionId"`                  // SessionID（已验证在线会话ID）。
	AssignmentID               string `json:"assignmentId"`               // AssignmentID（已验证目标Assignment）。
	DestinationWorldID         string `json:"destinationWorldId"`         // DestinationWorldID（目标世界ID）。
	DestinationExperienceID    string `json:"destinationExperienceId"`    // DestinationExperienceID（目标Experience）。
	MatchID                    string `json:"matchId,omitempty"`          // MatchID（目标比赛ID）。
	GameSessionID              string `json:"gameSessionId"`              // GameSessionID（已验证游戏会话绑定ID）。
	DestinationServerBootID    string `json:"destinationServerBootId"`    // DestinationServerBootID（已验证目标服务器Boot身份）。
	DestinationProtocolVersion uint32 `json:"destinationProtocolVersion"` // DestinationProtocolVersion（已验证协议版本）。
	SessionEpoch               uint64 `json:"sessionEpoch"`               // SessionEpoch（已验证会话绑定代次）。
}

// ReplayStore（防重放状态仓储）必须以原子SET-IF-ABSENT语义消费TicketID。
// 生产环境使用Redis SET NX + TTL；本地和单元测试使用MemoryReplayStore。
type ReplayStore interface {
	Consume(ctx context.Context, ticketID string, ttl time.Duration) (consumed bool, err error)
}

// SessionEpochStore（会话代次仓储）为每个在线Session分配严格递增的权威Epoch。
// 生产环境必须使用跨副本共享的原子INCR实现；本地测试可使用内存实现。
type SessionEpochStore interface {
	Next(ctx context.Context, sessionID string) (uint64, error)
}

// Service（迁移票据服务）使用HMAC-SHA256签名，并通过ReplayStore原子消费TicketID。
type Service struct {
	secret []byte
	now    func() time.Time
	replay ReplayStore
	epochs SessionEpochStore
}

// NewService（创建迁移服务）使用内存ReplayStore，兼容本地开发和既有调用。
// 生产装配必须使用NewServiceWithReplayStore注入Redis ReplayStore。
func NewService(secret []byte, now func() time.Time) *Service {
	return NewServiceWithStores(
		secret,
		now,
		NewMemoryReplayStore(),
		NewMemorySessionEpochStore())
}

// NewServiceWithReplayStore（兼容旧调用的构造器）保留外部ReplayStore，Epoch使用本进程内存。
// 生产多副本装配不得使用本入口，必须调用NewServiceWithStores注入共享EpochStore。
func NewServiceWithReplayStore(secret []byte, now func() time.Time, replay ReplayStore) *Service {
	return NewServiceWithStores(secret, now, replay, NewMemorySessionEpochStore())
}

// NewServiceWithStores（生产构造器）同时注入防重放仓储与会话代次仓储。
func NewServiceWithStores(
	secret []byte,
	now func() time.Time,
	replay ReplayStore,
	epochs SessionEpochStore,
) *Service {
	if len(secret) < 32 {
		panic("ServerTransfer签名密钥至少32字节")
	}
	if now == nil || replay == nil || epochs == nil {
		panic("ServerTransfer时钟、ReplayStore和SessionEpochStore不能为空")
	}
	return &Service{
		secret: append([]byte(nil), secret...),
		now:    now,
		replay: replay,
		epochs: epochs,
	}
}

// Issue（签发迁移票据）保留无Context兼容入口。
func (s *Service) Issue(req IssueRequest) (Ticket, error) {
	return s.IssueContext(context.Background(), req)
}

// IssueContext（签发迁移票据）在签票时就分配权威GameSessionID和单调SessionEpoch。
// 未使用或过期票据允许“烧掉”一个Epoch；Epoch只要求单调，不要求连续。
func (s *Service) IssueContext(ctx context.Context, req IssueRequest) (Ticket, error) {
	if req.TicketID == "" || req.PlayerID == "" || req.SessionID == "" ||
		req.DestinationGameServerID == "" || req.DestinationEndpoint == "" ||
		req.DestinationWorldID == "" || req.DestinationServerBootID == "" ||
		req.DestinationProtocolVersion == 0 {
		return Ticket{}, errors.New("迁移票据关键字段、ServerBootID和ProtocolVersion不能为空")
	}
	if req.AssignmentID == "" {
		req.AssignmentID = "legacy:" + req.TicketID
	}
	if req.DestinationExperienceID == "" {
		if req.MatchID != "" {
			req.DestinationExperienceID = "Experience.MainArena.Main"
		} else {
			req.DestinationExperienceID = "Experience.OpenWorld.Main"
		}
	}
	if req.TTL <= 0 || req.TTL > 5*time.Minute {
		return Ticket{}, errors.New("迁移票据TTL必须在0到5分钟之间")
	}
	epoch, err := s.epochs.Next(ctx, req.SessionID)
	if err != nil {
		return Ticket{}, fmt.Errorf("分配SessionEpoch失败: %w", err)
	}
	if epoch == 0 {
		return Ticket{}, errors.New("SESSION_EPOCH_INVALID: SessionEpoch必须大于0")
	}
	gameSessionID := fmt.Sprintf("game-session:%s:%d", req.SessionID, epoch)
	nonce, err := randomHex(16)
	if err != nil {
		return Ticket{}, err
	}
	now := s.now().UTC()
	ticket := Ticket{
		TicketID: req.TicketID, AssignmentID: req.AssignmentID, GameID: req.GameID, PlayerID: req.PlayerID, SessionID: req.SessionID,
		SourceGameServerID: req.SourceGameServerID, DestinationGameServerID: req.DestinationGameServerID,
		DestinationEndpoint: req.DestinationEndpoint, DestinationWorldID: req.DestinationWorldID,
		DestinationExperienceID: req.DestinationExperienceID, MatchID: req.MatchID,
		GameSessionID:              gameSessionID,
		DestinationServerBootID:    req.DestinationServerBootID,
		DestinationProtocolVersion: req.DestinationProtocolVersion,
		SessionEpoch:               epoch,
		IssuedAt:                   now, ExpiresAt: now.Add(req.TTL), Nonce: nonce,
	}
	ticket.Signature = s.sign(ticket)
	return ticket, nil
}

// Validate（验证迁移票据）保留无Context调用兼容入口；生产传输层应优先使用ValidateContext。
func (s *Service) Validate(req ValidateRequest) (ValidationResult, error) {
	return s.ValidateContext(context.Background(), req)
}

// ValidateContext（验证迁移票据）校验签名、过期时间、目标服务器并原子消费TicketID。
// 只有全部静态校验通过后才写ReplayStore，因此错误目标服务器不会意外烧掉合法票据。
func (s *Service) ValidateContext(ctx context.Context, req ValidateRequest) (ValidationResult, error) {
	ticket := req.Ticket
	if ticket.TicketID == "" || ticket.AssignmentID == "" ||
		ticket.DestinationExperienceID == "" || ticket.GameSessionID == "" ||
		ticket.DestinationServerBootID == "" || ticket.DestinationProtocolVersion == 0 ||
		ticket.SessionEpoch == 0 {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_INVALID: 迁移票据缺少关键会话绑定上下文")
	}
	if ticket.DestinationGameServerID != req.DestinationGameServerID {
		return ValidationResult{}, errors.New("TRANSFER_DESTINATION_MISMATCH: 票据目标服务器不匹配")
	}
	if req.DestinationServerBootID == "" ||
		ticket.DestinationServerBootID != req.DestinationServerBootID {
		return ValidationResult{}, errors.New("TRANSFER_SERVER_BOOT_MISMATCH: 票据目标服务器Boot身份已过期")
	}
	if req.DestinationProtocolVersion == 0 ||
		ticket.DestinationProtocolVersion != req.DestinationProtocolVersion {
		return ValidationResult{}, errors.New("TRANSFER_PROTOCOL_MISMATCH: 票据网络协议版本与目标服务器不一致")
	}
	now := s.now().UTC()
	if !now.Before(ticket.ExpiresAt) {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_EXPIRED: 迁移票据已过期")
	}
	if ticket.IssuedAt.After(now.Add(30 * time.Second)) {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_INVALID: 迁移票据签发时间异常")
	}
	if !hmac.Equal([]byte(ticket.Signature), []byte(s.sign(ticket))) {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_INVALID: 迁移票据签名无效")
	}
	ttl := ticket.ExpiresAt.Sub(now)
	if ttl <= 0 {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_EXPIRED: 迁移票据已过期")
	}
	consumed, err := s.replay.Consume(ctx, ticket.TicketID, ttl)
	if err != nil {
		return ValidationResult{}, err
	}
	if !consumed {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_ALREADY_USED: 迁移票据已经使用")
	}
	return ValidationResult{
		PlayerID:                   ticket.PlayerID,
		SessionID:                  ticket.SessionID,
		AssignmentID:               ticket.AssignmentID,
		DestinationWorldID:         ticket.DestinationWorldID,
		DestinationExperienceID:    ticket.DestinationExperienceID,
		MatchID:                    ticket.MatchID,
		GameSessionID:              ticket.GameSessionID,
		DestinationServerBootID:    ticket.DestinationServerBootID,
		DestinationProtocolVersion: ticket.DestinationProtocolVersion,
		SessionEpoch:               ticket.SessionEpoch,
	}, nil
}

func (s *Service) sign(ticket Ticket) string {
	unsigned := ticket
	unsigned.Signature = ""
	payload, _ := json.Marshal(unsigned)
	mac := hmac.New(sha256.New, s.secret)
	_, _ = mac.Write(payload)
	return hex.EncodeToString(mac.Sum(nil))
}

func randomHex(bytes int) (string, error) {
	buf := make([]byte, bytes)
	if _, err := rand.Read(buf); err != nil {
		return "", err
	}
	return hex.EncodeToString(buf), nil
}

// MemorySessionEpochStore（内存会话代次仓储）用于单进程开发与测试。
type MemorySessionEpochStore struct {
	mu     sync.Mutex
	epochs map[string]uint64
}

func NewMemorySessionEpochStore() *MemorySessionEpochStore {
	return &MemorySessionEpochStore{epochs: map[string]uint64{}}
}

func (s *MemorySessionEpochStore) Next(_ context.Context, sessionID string) (uint64, error) {
	if sessionID == "" {
		return 0, errors.New("SessionEpochStore的SessionID不能为空")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	current := s.epochs[sessionID]
	if current == ^uint64(0) {
		return 0, errors.New("SESSION_EPOCH_EXHAUSTED: SessionEpoch已耗尽")
	}
	current++
	s.epochs[sessionID] = current
	return current, nil
}

// MemoryReplayStore（内存防重放仓储）用于本地开发与单元测试。
type MemoryReplayStore struct {
	mu       sync.Mutex
	consumed map[string]time.Time
}

// NewMemoryReplayStore（创建内存防重放仓储）创建线程安全存储。
func NewMemoryReplayStore() *MemoryReplayStore {
	return &MemoryReplayStore{consumed: map[string]time.Time{}}
}

// Consume（原子消费TicketID）模拟Redis SET NX + TTL语义。
func (s *MemoryReplayStore) Consume(_ context.Context, ticketID string, ttl time.Duration) (bool, error) {
	if ticketID == "" || ttl <= 0 {
		return false, errors.New("ReplayStore的TicketID和TTL必须有效")
	}
	now := time.Now().UTC()
	s.mu.Lock()
	defer s.mu.Unlock()
	for id, expiresAt := range s.consumed {
		if !now.Before(expiresAt) {
			delete(s.consumed, id)
		}
	}
	if _, exists := s.consumed[ticketID]; exists {
		return false, nil
	}
	s.consumed[ticketID] = now.Add(ttl)
	return true, nil
}
