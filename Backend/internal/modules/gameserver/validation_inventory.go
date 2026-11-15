package gameserver

import (
	"sort"
	"time"
)

// ValidationInstances（本地验证实例快照）列出当前 Village 和 MainArena 启动或分配候选。
// MainArena 本地分配器仍可选超时的 Ready 实例，所以也返回它们供启动器拒绝。
func (r *Registry) ValidationInstances(now time.Time) []Instance {
	r.mu.RLock()
	defer r.mu.RUnlock()
	instances := make([]Instance, 0)
	for _, instance := range r.instances {
		if instance.RoleID != RoleMainArena && (instance.RoleID != RoleVillage || !hasFreshWorldHeartbeat(instance, now)) {
			continue
		}
		switch instance.Status {
		case StatusStarting:
			if hasFreshWorldHeartbeat(instance, now) {
				instances = append(instances, instance)
			}
		case StatusReady, StatusActive:
			instances = append(instances, instance)
		}
	}
	sort.Slice(instances, func(i, j int) bool { return instances[i].ID < instances[j].ID })
	return instances
}
