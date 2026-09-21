//go:build productiondeps

package redisstore

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"

	"divinebeasts/backend/internal/app/matchapi"
	"divinebeasts/backend/internal/modules/identity"
	"divinebeasts/backend/internal/modules/matchmaking"
	"divinebeasts/backend/internal/modules/party"
	"divinebeasts/backend/internal/modules/servertransfer"
)

const (
	sessionKeyPrefix        = "identity:session:"
	accessKeyPrefix         = "identity:access:"
	refreshKeyPrefix        = "identity:refresh:"
	partyKeyPrefix          = "party:"
	matchRequestKeyPrefix   = "matchmaking:request:"
	transferReplayKeyPrefix = "transfer:consumed:"
)

// SessionRepository（Redis会话仓储）实现identity.SessionRepository。
type SessionRepository struct{ client *Client }

// NewSessionRepository（创建Redis身份会话仓储）创建生产SessionRepository。
func NewSessionRepository(client *Client) *SessionRepository {
	return &SessionRepository{client: client}
}

func (r *SessionRepository) Save(ctx context.Context, session identity.Session) error {
	if session.SessionID == "" || session.AccessToken == "" || session.RefreshToken == "" {
		return errors.New("Identity Session关键字段不能为空")
	}
	payload, err := json.Marshal(session)
	if err != nil {
		return err
	}
	now := time.Now().UTC()
	refreshTTL := time.Until(session.RefreshExpiresAt)
	accessTTL := time.Until(session.AccessExpiresAt)
	if refreshTTL <= 0 || accessTTL <= 0 {
		return errors.New("Identity Session TTL无效")
	}
	// 先读取旧会话以删除旧Token索引，保证Refresh Rotation不会留下可重放Token。
	if oldPayload, err := r.client.inner.Get(ctx, sessionKeyPrefix+session.SessionID).Bytes(); err == nil {
		var old identity.Session
		if json.Unmarshal(oldPayload, &old) == nil {
			_ = r.client.inner.Del(ctx, accessKeyPrefix+old.AccessToken, refreshKeyPrefix+old.RefreshToken).Err()
		}
	}
	pipe := r.client.inner.TxPipeline()
	pipe.Set(ctx, sessionKeyPrefix+session.SessionID, payload, refreshTTL)
	pipe.Set(ctx, accessKeyPrefix+session.AccessToken, session.SessionID, accessTTL)
	pipe.Set(ctx, refreshKeyPrefix+session.RefreshToken, session.SessionID, refreshTTL)
	_, err = pipe.Exec(ctx)
	_ = now
	return err
}

func (r *SessionRepository) GetByRefreshToken(ctx context.Context, refreshToken string) (identity.Session, error) {
	return r.byIndex(ctx, refreshKeyPrefix+refreshToken)
}

func (r *SessionRepository) GetByAccessToken(ctx context.Context, accessToken string) (identity.Session, error) {
	return r.byIndex(ctx, accessKeyPrefix+accessToken)
}

func (r *SessionRepository) byIndex(ctx context.Context, indexKey string) (identity.Session, error) {
	sessionID, err := r.client.inner.Get(ctx, indexKey).Result()
	if errors.Is(err, redis.Nil) {
		return identity.Session{}, errors.New("AUTH_SESSION_INVALID: Token无效")
	}
	if err != nil {
		return identity.Session{}, err
	}
	payload, err := r.client.inner.Get(ctx, sessionKeyPrefix+sessionID).Bytes()
	if errors.Is(err, redis.Nil) {
		return identity.Session{}, errors.New("AUTH_SESSION_INVALID: Session不存在")
	}
	if err != nil {
		return identity.Session{}, err
	}
	var session identity.Session
	if err := json.Unmarshal(payload, &session); err != nil {
		return identity.Session{}, err
	}
	return session, nil
}

func (r *SessionRepository) Revoke(ctx context.Context, sessionID string) error {
	payload, err := r.client.inner.Get(ctx, sessionKeyPrefix+sessionID).Bytes()
	if errors.Is(err, redis.Nil) {
		return nil
	}
	if err != nil {
		return err
	}
	var session identity.Session
	if err := json.Unmarshal(payload, &session); err != nil {
		return err
	}
	return r.client.inner.Del(ctx, sessionKeyPrefix+sessionID, accessKeyPrefix+session.AccessToken, refreshKeyPrefix+session.RefreshToken).Err()
}

// PartyRepository（Redis Party仓储）实现matchapi.PartyRepository。
type PartyRepository struct {
	client *Client
	ttl    time.Duration
}

func NewPartyRepository(client *Client, ttl time.Duration) *PartyRepository {
	return &PartyRepository{client: client, ttl: ttl}
}

func (r *PartyRepository) Get(ctx context.Context, partyID string) (*party.Party, error) {
	payload, err := r.client.inner.Get(ctx, partyKeyPrefix+partyID).Bytes()
	if errors.Is(err, redis.Nil) {
		return nil, errors.New("PARTY_NOT_FOUND: Party不存在")
	}
	if err != nil {
		return nil, err
	}
	var value party.Party
	if err := json.Unmarshal(payload, &value); err != nil {
		return nil, err
	}
	return &value, nil
}

func (r *PartyRepository) Save(ctx context.Context, value *party.Party) error {
	if value == nil || value.ID == "" {
		return errors.New("Party不能为空")
	}
	payload, err := json.Marshal(value)
	if err != nil {
		return err
	}
	return r.client.inner.Set(ctx, partyKeyPrefix+value.ID, payload, r.ttl).Err()
}

// TicketRepository（Redis匹配幂等仓储）实现matchapi.TicketRepository。
type TicketRepository struct {
	client *Client
	ttl    time.Duration
}

func NewTicketRepository(client *Client, ttl time.Duration) *TicketRepository {
	return &TicketRepository{client: client, ttl: ttl}
}

func (r *TicketRepository) GetByClientRequestID(ctx context.Context, clientRequestID string) (matchmaking.Ticket, bool, error) {
	payload, err := r.client.inner.Get(ctx, matchRequestKeyPrefix+clientRequestID).Bytes()
	if errors.Is(err, redis.Nil) {
		return matchmaking.Ticket{}, false, nil
	}
	if err != nil {
		return matchmaking.Ticket{}, false, err
	}
	var ticket matchmaking.Ticket
	if err := json.Unmarshal(payload, &ticket); err != nil {
		return matchmaking.Ticket{}, false, err
	}
	return ticket, true, nil
}

func (r *TicketRepository) Save(ctx context.Context, clientRequestID string, ticket matchmaking.Ticket) error {
	payload, err := json.Marshal(ticket)
	if err != nil {
		return err
	}
	key := matchRequestKeyPrefix + clientRequestID
	created, err := r.client.inner.SetNX(ctx, key, payload, r.ttl).Result()
	if err != nil {
		return err
	}
	if created {
		return nil
	}
	current, found, err := r.GetByClientRequestID(ctx, clientRequestID)
	if err != nil {
		return err
	}
	if !found || current.TicketID != ticket.TicketID {
		return errors.New("MATCH_IDEMPOTENCY_CONFLICT: ClientRequestID已对应其他Ticket")
	}
	return nil
}

// TransferReplayStore（Redis迁移票据防重放仓储）使用SET NX + TTL原子消费TicketID。
type TransferReplayStore struct{ client *Client }

func NewTransferReplayStore(client *Client) *TransferReplayStore {
	return &TransferReplayStore{client: client}
}

// Consume（消费TicketID）返回true表示首次消费，false表示已被其他请求消费。
func (s *TransferReplayStore) Consume(ctx context.Context, ticketID string, ttl time.Duration) (bool, error) {
	if ticketID == "" || ttl <= 0 {
		return false, errors.New("Transfer Replay TicketID和TTL必须有效")
	}
	ok, err := s.client.inner.SetNX(ctx, transferReplayKeyPrefix+ticketID, "1", ttl).Result()
	if err != nil {
		return false, fmt.Errorf("Redis消费TransferTicket失败: %w", err)
	}
	return ok, nil
}

var _ identity.SessionRepository = (*SessionRepository)(nil)
var _ matchapi.PartyRepository = (*PartyRepository)(nil)
var _ matchapi.TicketRepository = (*TicketRepository)(nil)
var _ servertransfer.ReplayStore = (*TransferReplayStore)(nil)
