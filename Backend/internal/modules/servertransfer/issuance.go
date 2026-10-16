// 本文件为迁移签票提供幂等意图仓储：同票据身份/内容只返回同一秘密与Epoch，恢复不能重复产生有效身份。
// 内存实现只供单进程开发/测试；生产必须在组合根注入共享Redis实现，秘密不得进入公共状态或日志。
package servertransfer

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sync"
	"time"
)

// IssuanceStore原子保存首次完整签票结果；create可因事务竞争重试，未提交Epoch允许烧号，不得向调用者发布失败分支凭据。
type IssuanceStore interface {
	GetOrIssue(context.Context, string, string, time.Time, func() (Ticket, error)) (Ticket, error)
}

// IssuanceReceipt仅后端短期保存，Fingerprint不含明文Token；Ticket含秘密，禁止格式化打印整个回执。
type IssuanceReceipt struct {
	Fingerprint string // 标准化受信签票参数摘要，同键异参冲突。
	Ticket      Ticket // 首次完整签票，仅受保护服务可以读取。
}

// MemoryIssuanceStore在一个服务作用域内拥有幂等结果；服务销毁释放，不能当成生产恢复仓储。
type MemoryIssuanceStore struct {
	mu    sync.Mutex
	items map[string]IssuanceReceipt
}

func NewMemoryIssuanceStore() *MemoryIssuanceStore {
	return &MemoryIssuanceStore{items: map[string]IssuanceReceipt{}}
}

// GetOrIssue在同锁保存首次结果；过期身份不自动生成新票，恢复方必须明确产生新的逻辑迁移身份。
func (s *MemoryIssuanceStore) GetOrIssue(ctx context.Context, id, fingerprint string, now time.Time, create func() (Ticket, error)) (Ticket, error) {
	if err := ctx.Err(); err != nil {
		return Ticket{}, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if receipt, found := s.items[id]; found {
		if receipt.Fingerprint != fingerprint {
			return Ticket{}, errors.New("TRANSFER_IDEMPOTENCY_CONFLICT: 同票据身份对应不同签票意图")
		}
		if !now.Before(receipt.Ticket.ExpiresAt) {
			return Ticket{}, errors.New("TRANSFER_TICKET_EXPIRED: 必须请求新的迁移身份")
		}
		return receipt.Ticket, nil
	}
	ticket, err := create()
	if err != nil {
		return Ticket{}, err
	}
	if err := ctx.Err(); err != nil {
		return Ticket{}, err
	}
	s.items[id] = IssuanceReceipt{Fingerprint: fingerprint, Ticket: ticket}
	return ticket, nil
}

// issuanceFingerprint只对固定受信输入计算摘要，不接收任意map或日志正文，序列化失败必须传播。
func issuanceFingerprint(request IssueRequest) (string, error) {
	payload, err := json.Marshal(request)
	if err != nil {
		return "", err
	}
	digest := sha256.Sum256(payload)
	return hex.EncodeToString(digest[:]), nil
}

// RevokeContext原子烧掉指定票据身份，覆盖“签票响应未知”时已经生成但还未持久的秘密；重复撤销幂等。
// 10分钟覆盖最大5分钟票据TTL和传输恢复窗口；只能由受信控制面调用，不对普通客户端暴露任意TicketID撤销。
func (s *Service) RevokeContext(ctx context.Context, ticketID string) error {
	if ticketID == "" || len(ticketID) > 128 {
		return errors.New("TRANSFER_TICKET_INVALID: 撤销身份无效")
	}
	_, err := s.replay.Consume(ctx, ticketID, 10*time.Minute)
	return err
}
