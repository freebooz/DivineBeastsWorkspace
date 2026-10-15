//go:build productiondeps

// 本文件是共享Redis签票意图回执；WATCH覆盖回执与撤销/已消费栅栏，只有EXEC成功的首次秘密才能返回。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/json"
	"errors"
	"github.com/redis/go-redis/v9"
	"time"
)

// TransferIssuanceStore与防重放/Epoch使用同一Redis写域，禁止跨槽Cluster下退化为多次SET。
type TransferIssuanceStore struct{ client *Client }

func NewTransferIssuanceStore(client *Client) *TransferIssuanceStore {
	return &TransferIssuanceStore{client: client}
}

// GetOrIssue同内容重试返回唯一首次凭据；过期或已撤销身份拒绝，不自动生成第二个Epoch。
func (s *TransferIssuanceStore) GetOrIssue(ctx context.Context, id, fingerprint string, now time.Time, create func() (servertransfer.Ticket, error)) (servertransfer.Ticket, error) {
	if _, cluster := s.client.inner.(*redis.ClusterClient); cluster {
		return servertransfer.Ticket{}, errors.New("TRANSFER_CLUSTER_UNSUPPORTED: 幂等签票要求单逻辑写域")
	}
	key := "transfer:issued:" + id
	replay := transferReplayKeyPrefix + id
	var result servertransfer.Ticket
	for attempt := 0; attempt < 8; attempt++ {
		err := s.client.inner.Watch(ctx, func(tx *redis.Tx) error {
			used, err := tx.Exists(ctx, replay).Result()
			if err != nil {
				return err
			}
			if used != 0 {
				return errors.New("TRANSFER_TICKET_ALREADY_USED: 此身份已消费或撤销")
			}
			raw, err := tx.Get(ctx, key).Bytes()
			if err == nil {
				var receipt servertransfer.IssuanceReceipt
				if err := json.Unmarshal(raw, &receipt); err != nil {
					return err
				}
				if receipt.Fingerprint != fingerprint {
					return errors.New("TRANSFER_IDEMPOTENCY_CONFLICT: 同身份对应不同签票意图")
				}
				if !now.Before(receipt.Ticket.ExpiresAt) {
					return errors.New("TRANSFER_TICKET_EXPIRED: 必须请求新的迁移身份")
				}
				result = receipt.Ticket
				return nil
			}
			if !errors.Is(err, redis.Nil) {
				return err
			}
			ticket, err := create()
			if err != nil {
				return err
			}
			if err := ctx.Err(); err != nil {
				return err
			}
			payload, err := json.Marshal(servertransfer.IssuanceReceipt{Fingerprint: fingerprint, Ticket: ticket})
			if err != nil {
				return err
			}
			_, err = tx.TxPipelined(ctx, func(pipe redis.Pipeliner) error { pipe.Set(ctx, key, payload, 10*time.Minute); return nil })
			if err == nil {
				result = ticket
			}
			return err
		}, key, replay)
		if !errors.Is(err, redis.TxFailedErr) {
			return result, err
		}
	}
	return servertransfer.Ticket{}, errors.New("TRANSFER_ISSUANCE_CONFLICT: 签票竞争，请沿原身份重试")
}

var _ servertransfer.IssuanceStore = (*TransferIssuanceStore)(nil)
