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
	"sync"
	"time"
)

// IssueRequest（迁移票据签发请求）描述玩家从当前服务器迁移到目标服务器所需上下文。
type IssueRequest struct {
	TicketID                string        // TicketID（迁移票据ID）。
	AssignmentID            string        // AssignmentID（目标世界或比赛分配ID）。
	GameID                  string        // GameID（游戏ID）。
	PlayerID                string        // PlayerID（玩家ID）。
	SessionID               string        // SessionID（在线会话ID）。
	SourceGameServerID      string        // SourceGameServerID（来源服务器实例ID，首次进服可为空）。
	DestinationGameServerID string        // DestinationGameServerID（目标服务器实例ID）。
	DestinationEndpoint     string        // DestinationEndpoint（ClientTravel目标地址）。
	DestinationWorldID      string        // DestinationWorldID（目标世界ID）。
	DestinationExperienceID string        // DestinationExperienceID（目标Experience，例如OpenWorld.Hub）。
	MatchID                 string        // MatchID（进入MainArena时的比赛ID；普通世界迁移为空）。
	TTL                     time.Duration // TTL（票据有效时长）。
}

// Ticket（迁移票据）是Game Client ClientTravel前获取的短期凭据。
type Ticket struct {
	TicketID                string    `json:"ticketId"`                     // TicketID（迁移票据唯一ID）。
	AssignmentID            string    `json:"assignmentId"`                 // AssignmentID（目标Assignment ID）。
	GameID                  string    `json:"gameId"`                       // GameID（游戏ID）。
	PlayerID                string    `json:"playerId"`                     // PlayerID（玩家ID）。
	SessionID               string    `json:"sessionId"`                    // SessionID（在线会话ID）。
	SourceGameServerID      string    `json:"sourceGameServerId,omitempty"` // SourceGameServerID（来源服务器实例ID）。
	DestinationGameServerID string    `json:"destinationGameServerId"`      // DestinationGameServerID（目标服务器实例ID）。
	DestinationEndpoint     string    `json:"destinationEndpoint"`          // DestinationEndpoint（目标连接地址）。
	DestinationWorldID      string    `json:"destinationWorldId"`           // DestinationWorldID（目标世界ID）。
	DestinationExperienceID string    `json:"destinationExperienceId"`      // DestinationExperienceID（目标Experience）。
	MatchID                 string    `json:"matchId,omitempty"`            // MatchID（比赛ID，普通世界迁移可为空）。
	IssuedAt                time.Time `json:"issuedAt"`                     // IssuedAt（签发时间）。
	ExpiresAt               time.Time `json:"expiresAt"`                    // ExpiresAt（过期时间）。
	Nonce                   string    `json:"nonce"`                        // Nonce（一次性随机值）。
	Signature               string    `json:"signature"`                    // Signature（HMAC-SHA256签名）。
}

// ValidateRequest（迁移票据验证请求）由目标Dedicated Server提交。
type ValidateRequest struct {
	Ticket                  Ticket // Ticket（待验证迁移票据）。
	DestinationGameServerID string // DestinationGameServerID（正在执行验证的目标服务器ID）。
}

// ValidationResult（迁移验证结果）返回目标服务器建立玩家会话所需的可信上下文。
type ValidationResult struct {
	PlayerID                string // PlayerID（已验证玩家ID）。
	SessionID               string // SessionID（已验证在线会话ID）。
	AssignmentID            string // AssignmentID（已验证目标Assignment）。
	DestinationWorldID      string // DestinationWorldID（目标世界ID）。
	DestinationExperienceID string // DestinationExperienceID（目标Experience）。
	MatchID                 string // MatchID（目标比赛ID）。
}

// ReplayStore（防重放状态仓储）必须以原子SET-IF-ABSENT语义消费TicketID。
// 生产环境使用Redis SET NX + TTL；本地和单元测试使用MemoryReplayStore。
type ReplayStore interface {
	Consume(ctx context.Context, ticketID string, ttl time.Duration) (consumed bool, err error)
}

// Service（迁移票据服务）使用HMAC-SHA256签名，并通过ReplayStore原子消费TicketID。
type Service struct {
	secret []byte
	now    func() time.Time
	replay ReplayStore
}

// NewService（创建迁移服务）使用内存ReplayStore，兼容本地开发和既有调用。
// 生产装配必须使用NewServiceWithReplayStore注入Redis ReplayStore。
func NewService(secret []byte, now func() time.Time) *Service {
	return NewServiceWithReplayStore(secret, now, NewMemoryReplayStore())
}

// NewServiceWithReplayStore（创建带外部防重放存储的迁移服务）要求至少32字节签名密钥。
func NewServiceWithReplayStore(secret []byte, now func() time.Time, replay ReplayStore) *Service {
	if len(secret) < 32 {
		panic("ServerTransfer签名密钥至少32字节")
	}
	if now == nil || replay == nil {
		panic("ServerTransfer时钟和ReplayStore不能为空")
	}
	return &Service{secret: append([]byte(nil), secret...), now: now, replay: replay}
}

// Issue（签发迁移票据）创建短期、目标绑定、带随机Nonce的迁移票据。
func (s *Service) Issue(req IssueRequest) (Ticket, error) {
	if req.TicketID == "" || req.PlayerID == "" || req.SessionID == "" || req.DestinationGameServerID == "" || req.DestinationEndpoint == "" || req.DestinationWorldID == "" {
		return Ticket{}, errors.New("迁移票据关键字段不能为空")
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
		IssuedAt: now, ExpiresAt: now.Add(req.TTL), Nonce: nonce,
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
	if ticket.TicketID == "" || ticket.AssignmentID == "" || ticket.DestinationExperienceID == "" {
		return ValidationResult{}, errors.New("TRANSFER_TICKET_INVALID: 迁移票据缺少关键上下文")
	}
	if ticket.DestinationGameServerID != req.DestinationGameServerID {
		return ValidationResult{}, errors.New("TRANSFER_DESTINATION_MISMATCH: 票据目标服务器不匹配")
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
		PlayerID: ticket.PlayerID, SessionID: ticket.SessionID, AssignmentID: ticket.AssignmentID,
		DestinationWorldID: ticket.DestinationWorldID, DestinationExperienceID: ticket.DestinationExperienceID, MatchID: ticket.MatchID,
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
