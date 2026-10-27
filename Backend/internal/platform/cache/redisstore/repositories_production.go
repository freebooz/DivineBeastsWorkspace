//go:build productiondeps

package redisstore

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
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
	sessionKeyPrefix               = "identity:session:"
	accessKeyPrefix                = "identity:access:"
	refreshKeyPrefix               = "identity:refresh:"
	revokedSessionKeyPrefix        = "identity:revoked:"
	partyKeyPrefix                 = "party:"
	matchRequestKeyPrefix          = "matchmaking:request:"
	transferReplayKeyPrefix        = "transfer:consumed:"
	transferEpochKeyPrefix         = "transfer:session-epoch:"
	transferAcceptedEpochKeyPrefix = "transfer:accepted-epoch:"
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
	// 游客兼容仓储也只保存摘要；刷新历史索引保留用于重放撤销与幂等退出。
	session.AccessToken, session.RefreshToken = sessionTokenDigest(session.AccessToken), sessionTokenDigest(session.RefreshToken)
	payload, err := json.Marshal(session)
	if err != nil {
		return err
	}
	refreshTTL := time.Until(session.RefreshExpiresAt)
	accessTTL := time.Until(session.AccessExpiresAt)
	if refreshTTL <= 0 || accessTTL <= 0 {
		return errors.New("Identity Session TTL无效")
	}
	// 先读取旧会话以删除旧Token索引，保证Refresh Rotation不会留下可重放Token。
	if oldPayload, err := r.client.inner.Get(ctx, sessionKeyPrefix+session.SessionID).Bytes(); err == nil {
		var old identity.Session
		if json.Unmarshal(oldPayload, &old) == nil {
			_ = r.client.inner.Del(ctx, accessKeyPrefix+old.AccessToken).Err()
		}
	}
	pipe := r.client.inner.TxPipeline()
	// 同SQL/内存仓储保留历史元数据，截止由唯一领域规则判断；不因TTL丢失退出幂等性。
	pipe.Set(ctx, sessionKeyPrefix+session.SessionID, payload, 0)
	pipe.Set(ctx, accessKeyPrefix+session.AccessToken, session.SessionID, accessTTL)
	pipe.Set(ctx, refreshKeyPrefix+session.RefreshToken, session.SessionID, 0)
	_, err = pipe.Exec(ctx)
	return err
}

func (r *SessionRepository) GetByRefreshToken(ctx context.Context, refreshToken string) (identity.Session, error) {
	s, err := r.byIndex(ctx, refreshKeyPrefix+sessionTokenDigest(refreshToken))
	if err == nil && s.RefreshToken != sessionTokenDigest(refreshToken) {
		return identity.Session{}, identity.ErrInvalidToken
	}
	return s, err
}

func (r *SessionRepository) GetByAccessToken(ctx context.Context, accessToken string) (identity.Session, error) {
	return r.byIndex(ctx, accessKeyPrefix+sessionTokenDigest(accessToken))
}

func (r *SessionRepository) byIndex(ctx context.Context, indexKey string) (identity.Session, error) {
	sessionID, err := r.client.inner.Get(ctx, indexKey).Result()
	if errors.Is(err, redis.Nil) {
		return identity.Session{}, errors.New("AUTH_SESSION_INVALID: Token无效")
	}
	if err != nil {
		return identity.Session{}, err
	}
	if revoked, err := r.client.inner.Exists(ctx, revokedSessionKeyPrefix+sessionID).Result(); err != nil {
		return identity.Session{}, err
	} else if revoked != 0 {
		return identity.Session{}, identity.ErrInvalidToken
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
	pipe := r.client.inner.TxPipeline()
	pipe.Set(ctx, revokedSessionKeyPrefix+sessionID, "1", 0)
	pipe.Del(ctx, accessKeyPrefix+session.AccessToken)
	_, err = pipe.Exec(ctx)
	return err
}

func sessionTokenDigest(token string) string {
	digest := sha256.Sum256([]byte(token))
	return hex.EncodeToString(digest[:])
}

// Rotate（Redis原子轮换）只适配WATCH/MULTI事务；截止和重放业务判断来自唯一领域规则。
func (r *SessionRepository) Rotate(ctx context.Context, digest string, n identity.TokenRotation) (identity.StoredSession, error) {
	started := time.Now()
	id, err := r.client.inner.Get(ctx, refreshKeyPrefix+digest).Result()
	if errors.Is(err, redis.Nil) {
		return identity.StoredSession{}, identity.ErrInvalidToken
	}
	if err != nil {
		return identity.StoredSession{}, err
	}
	if err := ctx.Err(); err != nil {
		return identity.StoredSession{}, err
	}
	key, revokedKey := sessionKeyPrefix+id, revokedSessionKeyPrefix+id
	for {
		var fresh identity.StoredSession
		var domainErr error
		err = r.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			if err := ctx.Err(); err != nil {
				return err
			}
			if revoked, e := tx.Exists(ctx, revokedKey).Result(); e != nil {
				return e
			} else if revoked != 0 {
				return identity.ErrInvalidToken
			}
			payload, e := tx.Get(ctx, key).Bytes()
			if errors.Is(e, redis.Nil) {
				return identity.ErrInvalidToken
			}
			if e != nil {
				return e
			}
			var previous identity.Session
			if e = json.Unmarshal(payload, &previous); e != nil {
				return e
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			rotation := n
			rotation.Now = rotation.Now.Add(time.Since(started))
			fresh, domainErr = rotation.Apply(identity.StoredSession{Session: previous, AccessDigest: previous.AccessToken, RefreshDigest: previous.RefreshToken}, digest)
			_, e = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
				if err := ctx.Err(); err != nil {
					return err
				}
				pipe.Del(ctx, accessKeyPrefix+previous.AccessToken)
				if domainErr != nil {
					pipe.Set(ctx, revokedKey, "1", 0)
					return nil
				}
				stored := fresh.Session
				stored.AccessToken, stored.RefreshToken = fresh.AccessDigest, fresh.RefreshDigest
				encoded, marshalErr := json.Marshal(stored)
				if marshalErr != nil {
					return marshalErr
				}
				pipe.Set(ctx, key, encoded, 0)
				pipe.Set(ctx, accessKeyPrefix+fresh.AccessDigest, id, 0)
				pipe.PExpireAt(ctx, accessKeyPrefix+fresh.AccessDigest, fresh.AccessExpiresAt)
				pipe.Set(ctx, refreshKeyPrefix+fresh.RefreshDigest, id, 0)
				return nil
			})
			return e
		}, key, revokedKey)
		if errors.Is(err, redis.TxFailedErr) {
			continue
		}
		if err != nil {
			return identity.StoredSession{}, err
		}
		if domainErr != nil {
			return identity.StoredSession{}, domainErr
		}
		return fresh, nil
	}
}

// Logout（历史凭据幂等退出）保留摘要归属，重复请求仍可定位原会话。
func (r *SessionRepository) Logout(ctx context.Context, digest string) error {
	id, err := r.client.inner.Get(ctx, refreshKeyPrefix+digest).Result()
	if errors.Is(err, redis.Nil) {
		return identity.ErrInvalidToken
	}
	if err != nil {
		return err
	}
	return r.client.inner.Set(ctx, revokedSessionKeyPrefix+id, "1", 0).Err()
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
		return nil, matchapi.ErrPartyNotFound
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
	return r.saveNewParty(ctx, value)
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

// TransferEpochStore（Redis会话代次仓储）使用INCR为每个在线Session分配全局单调Epoch。
// Key TTL大于身份刷新会话上限，避免长期孤儿键；在会话仍有效期间不会回绕。
type TransferEpochStore struct{ client *Client }

func NewTransferEpochStore(client *Client) *TransferEpochStore {
	return &TransferEpochStore{client: client}
}

func (s *TransferEpochStore) Next(ctx context.Context, sessionID string) (uint64, error) {
	if sessionID == "" {
		return 0, errors.New("Transfer Epoch SessionID不能为空")
	}
	key := transferEpochKeyPrefix + sessionID
	value, err := s.client.inner.Incr(ctx, key).Result()
	if err != nil {
		return 0, fmt.Errorf("Redis分配SessionEpoch失败: %w", err)
	}
	if value <= 0 {
		return 0, errors.New("Redis返回无效SessionEpoch")
	}
	// 身份Refresh Session最长30天；45天过期仅清理已失效会话的孤儿计数器。
	_ = s.client.inner.Expire(ctx, key, 45*24*time.Hour).Err()
	return uint64(value), nil
}

// Accept（确认服务端准入Epoch）使用Lua在Redis内原子执行仅前进比较。
// Epoch按十进制字符串长度+字典序比较，避免Lua number在2^53以上丢失uint64精度。
func (s *TransferEpochStore) Accept(
	ctx context.Context,
	sessionID string,
	epoch uint64,
	ticketID string,
	ttl time.Duration,
) (bool, error) {
	if sessionID == "" || epoch == 0 || ticketID == "" || ttl <= 0 {
		return false, errors.New("Transfer accepted Epoch参数无效")
	}

	const script = `
local current = redis.call("GET", KEYS[1])
local latest_issued = redis.call("GET", KEYS[2])
local proposed_epoch = ARGV[1]
local proposed_ticket = ARGV[2]
local ttl_ms = ARGV[3]
local proposed_value = proposed_epoch .. "|" .. proposed_ticket

-- 十进制无符号整数比较：长度优先，同长度按字典序。
-- 更高Epoch一旦被签发，低Epoch即失效，不等待新票先完成准入。
if latest_issued then
  if string.len(proposed_epoch) < string.len(latest_issued) then
    return 0
  end
  if string.len(proposed_epoch) == string.len(latest_issued) and proposed_epoch < latest_issued then
    return 0
  end
end

if not current then
  redis.call("PSETEX", KEYS[1], ttl_ms, proposed_value)
  return 1
end

local sep = string.find(current, "|", 1, true)
if not sep then
  return redis.error_reply("accepted epoch value corrupt")
end

local current_epoch = string.sub(current, 1, sep - 1)
local current_ticket = string.sub(current, sep + 1)

if string.len(proposed_epoch) < string.len(current_epoch) then
  return 0
end
if string.len(proposed_epoch) == string.len(current_epoch) and proposed_epoch < current_epoch then
  return 0
end
if proposed_epoch == current_epoch then
  if proposed_ticket ~= current_ticket then
    return 0
  end
  redis.call("PEXPIRE", KEYS[1], ttl_ms)
  return 1
end

redis.call("PSETEX", KEYS[1], ttl_ms, proposed_value)
return 1
`

	value, err := s.client.inner.Eval(
		ctx,
		script,
		[]string{
			transferAcceptedEpochKeyPrefix + sessionID,
			transferEpochKeyPrefix + sessionID,
		},
		fmt.Sprintf("%d", epoch),
		ticketID,
		ttl.Milliseconds(),
	).Int()
	if err != nil {
		return false, fmt.Errorf("Redis确认SessionEpoch失败: %w", err)
	}
	return value == 1, nil
}

var _ identity.SessionRepository = (*SessionRepository)(nil)
var _ matchapi.PartyRepository = (*PartyRepository)(nil)
var _ matchapi.TicketRepository = (*TicketRepository)(nil)
var _ servertransfer.ReplayStore = (*TransferReplayStore)(nil)
var _ servertransfer.SessionEpochStore = (*TransferEpochStore)(nil)
