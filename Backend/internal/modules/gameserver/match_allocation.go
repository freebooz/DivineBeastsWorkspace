// 本文件为MainArena分配提供同比赛身份的幂等读取，避免“分配已生效但响应丢失”时重复占用服务器。
package gameserver

import "errors"

// FindMatchAllocation供外部分配器在调用Agones前复查已确认绑定；多个同Match绑定失败关闭，不任意选实例。
func (r *Registry) FindMatchAllocation(request AllocationRequest) (Instance, bool, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.findMatchAllocationLocked(request)
}

// findMatchAllocationLocked要求调用者持有注册表读/写锁；重试角色、区域、体验与人数都必须保持原意图。
func (r *Registry) findMatchAllocationLocked(request AllocationRequest) (Instance, bool, error) {
	if request.MatchID == "" || request.RoleID != RoleMainArena {
		return Instance{}, false, nil
	}
	var selected Instance
	found := false
	for _, instance := range r.instances {
		if instance.CurrentMatchID != request.MatchID {
			continue
		}
		if found {
			return Instance{}, false, errors.New("GAME_SERVER_MATCH_CONFLICT: 同比赛绑定了多个实例")
		}
		if instance.RoleID != request.RoleID || instance.RegionID != request.RegionID || (request.ExperienceID != "" && instance.ExperienceID != request.ExperienceID) || instance.AllocatedMatchPlayers != request.RequiredCapacity {
			return Instance{}, false, errors.New("GAME_SERVER_IDEMPOTENCY_CONFLICT: 同比赛分配意图发生变化")
		}
		if instance.Status != StatusActive {
			return Instance{}, false, errors.New("GAME_SERVER_NOT_ACTIVE: 原分配已失效")
		}
		selected = instance
		found = true
	}
	return selected, found, nil
}
