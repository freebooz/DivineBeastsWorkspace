// 认证连接续租与截止收敛由应用服务持有；跨地图GI连续观察，不因Widget开关或一次请求取消判离线。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
	"errors"
	"strings"
	"time"
)

// renewPartyObservation在同一聚合事务更新连接租约；只有可见状态变化发事件，Revision冲突有界重读。
func (s *Service) renewPartyObservation(ctx context.Context, actor string, now time.Time) error {
	repo, ok := s.parties.(AtomicPartyRepository)
	if !ok {
		return errors.New("PARTY_ATOMIC_STORE_REQUIRED: 未装配事务")
	}
	for attempt := 0; attempt < 8; attempt++ {
		p, err := repo.GetByPlayerID(ctx, actor)
		if errors.Is(err, ErrPartyNotFound) {
			return nil
		}
		if err != nil {
			return err
		}
		_, err = repo.Update(ctx, p.ID, p.Revision, "", "", func(current *party.Party) error {
			current.RenewPresence(actor, now)
			current.ReconcilePresence(now)
			return current.TransferOfflineLeadership(now)
		})
		if err == nil {
			return nil
		}
		if !strings.HasPrefix(err.Error(), "PARTY_REVISION_CONFLICT:") {
			return err
		}
	}
	return errors.New("PARTY_REVISION_CONFLICT: 连接续租持续冲突")
}
