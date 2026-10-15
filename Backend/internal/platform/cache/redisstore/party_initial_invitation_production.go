//go:build productiondeps

// 本文件复用普通队伍WATCH/EXEC写域，原子提交无队伍邀请、成员索引、收件箱、通知和幂等回执。
package redisstore

import (
	"context"
	"divinebeasts/backend/internal/modules/party"
)

// CreateInvitedParty不先写空队伍；共享事务按实际初始名单检查玩家预留和唯一归属。
func (r *PartyRepository) CreateInvitedParty(ctx context.Context, value *party.Party, key, fingerprint string) (*party.Party, error) {
	if err := r.requirePartyIndex(ctx); err != nil {
		return nil, err
	}
	return r.updateParty(ctx, value.ID, 0, key, fingerprint, true, func(next *party.Party) error { *next = *value.Clone(); return nil })
}
