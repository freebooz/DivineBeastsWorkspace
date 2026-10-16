// 本文件将返回目标选择持久化在签票之前；跨进程重试读取同一意图，不重复BestFit分配或改变目标签名。
package gameservercontrol

import (
	"context"
	"crypto/sha256"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
	"encoding/hex"
	"encoding/json"
	"errors"
	"sync"
	"time"
)

// WorldAllocationRecord不含秘密，持久目标Boot及期限；签票回执另由统一Transfer仓储持有。
type WorldAllocationRecord struct {
	Fingerprint  string             // 固定输入摘要；同TicketID异内容冲突。
	Assignment   gc.WorldAssignment // 首次选定的世界任务及端点，只供受保护签票。
	ServerBootID string             // 目标真实启动代次；重启后拒绝继承。
	ExpiresAtUTC time.Time          // 原意图UTC截止；过期需新的逻辑请求。
}

// WorldAllocationStore的create必须只有快照选择，禁止副作用；并发提交只允许唯一首次目标。
type WorldAllocationStore interface {
	GetOrCreateWorldAllocation(context.Context, string, string, time.Time, func() (WorldAllocationRecord, error)) (WorldAllocationRecord, error)
}

// memoryWorldAllocationStore仅供明确单进程开发/测试，不能作为生产跨副本恢复仓储。
type memoryWorldAllocationStore struct {
	mu    sync.Mutex
	items map[string]WorldAllocationRecord
}

func (s *memoryWorldAllocationStore) GetOrCreateWorldAllocation(ctx context.Context, id, fp string, now time.Time, create func() (WorldAllocationRecord, error)) (WorldAllocationRecord, error) {
	if err := ctx.Err(); err != nil {
		return WorldAllocationRecord{}, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if v, ok := s.items[id]; ok {
		if v.Fingerprint != fp {
			return v, errors.New("TRANSFER_IDEMPOTENCY_CONFLICT: 世界返回意图变化")
		}
		if !now.Before(v.ExpiresAtUTC) {
			return v, errors.New("TRANSFER_TICKET_EXPIRED: 返回意图过期")
		}
		return v, nil
	}
	for key, v := range s.items {
		if !now.Before(v.ExpiresAtUTC.Add(10 * time.Minute)) {
			delete(s.items, key)
		}
	}
	if len(s.items) >= 65536 {
		return WorldAllocationRecord{}, errors.New("WORLD_ALLOCATION_CAPACITY: 恢复窗口已满")
	}
	v, err := create()
	if err != nil {
		return v, err
	}
	if err := ctx.Err(); err != nil {
		return v, err
	}
	s.items[id] = v
	return v, nil
}

// prepareWorldTransfer先保存目标，之后容量条件确认失败保持原意图，不在未知响应中悄悄切换服务器。
func (s *Service) prepareWorldTransfer(ctx context.Context, input AllocateWorldTransferInput) (WorldAllocationRecord, error) {
	payload, err := json.Marshal(input)
	if err != nil {
		return WorldAllocationRecord{}, err
	}
	digest := sha256.Sum256(payload)
	fp := hex.EncodeToString(digest[:])
	now := s.now().UTC()
	return s.worldAllocations.GetOrCreateWorldAllocation(ctx, input.Transfer.TicketID, fp, now, func() (WorldAllocationRecord, error) {
		role, ok := gc.RoleForExperience(input.World.ExperienceID)
		if !ok {
			return WorldAllocationRecord{}, errors.New("WORLD_ASSIGNMENT_INVALID: 未登记体验")
		}
		target, err := s.registry.PreviewWorldAllocation(gameserver.AllocationRequest{RoleID: role, ExperienceID: input.World.ExperienceID, WorldID: input.World.WorldID, RegionID: input.World.RegionID, RequiredCapacity: input.World.PlayerSlots}, now)
		if err != nil {
			return WorldAllocationRecord{}, err
		}
		assignment := gc.WorldAssignment{AssignmentID: "world:" + target.ID + ":" + input.World.ExperienceID, GameServerID: target.ID, ServerRoleID: role, ExperienceID: input.World.ExperienceID, WorldID: input.World.WorldID, RegionID: input.World.RegionID, Endpoint: target.PublicEndpoint}
		if err := assignment.Validate(); err != nil {
			return WorldAllocationRecord{}, err
		}
		return WorldAllocationRecord{Fingerprint: fp, Assignment: assignment, ServerBootID: target.ServerBootID, ExpiresAtUTC: now.Add(input.Transfer.TTL)}, nil
	})
}
