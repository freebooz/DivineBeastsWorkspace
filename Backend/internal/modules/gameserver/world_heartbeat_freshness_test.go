// 本回归复现已退出Village仍被分配：只使用真实Registry和控制面时钟，不探测网络、不伪造心跳。
package gameserver

import (
	"testing"
	"time"
)

// 死实例ID字典序更小且容量更合适，也不得盖过新实例；没有新鲜候选时必须拒绝分配。
func TestWorldAllocationIgnoresExpiredHeartbeat(t *testing.T) {
	now := time.Now().UTC()
	r := NewRegistry()
	req := AllocationRequest{RoleID: RoleVillage, ExperienceID: "tutorial", WorldID: "village", RegionID: "local", RequiredCapacity: 1}
	for _, s := range []Instance{
		{ID: "a-old", ServerBootID: "boot-old", RoleID: RoleVillage, ExperienceID: "tutorial", WorldID: "village", RegionID: "local", Capacity: 2, Status: StatusReady, LastHeartbeat: now.Add(-time.Hour)},
		{ID: "z-live", ServerBootID: "boot-live", RoleID: RoleVillage, ExperienceID: "tutorial", WorldID: "village", RegionID: "local", Capacity: 20, Status: StatusReady, LastHeartbeat: now},
	} {
		r.Register(s)
	}
	got, err := r.PreviewWorldAllocation(req, now)
	if err != nil || got.ID != "z-live" {
		t.Fatalf("必须选择新鲜实例，got=%s err=%v", got.ID, err)
	}
	if _, err := r.PreviewWorldAllocation(req, now.Add(31*time.Second)); err == nil {
		t.Fatal("所有心跳过期后不能继续签发世界分配")
	}
}

// 预览到预留之间失去心跳也要拒绝；拒绝不能增加预留容量。
func TestWorldReservationRechecksHeartbeat(t *testing.T) {
	now := time.Now().UTC()
	r := NewRegistry()
	r.Register(Instance{ID: "village", ServerBootID: "boot", RoleID: RoleVillage, Capacity: 2, Status: StatusReady, LastHeartbeat: now})
	if err := r.ReserveWorldTransfer("ticket", "village", "boot", 1, now.Add(31*time.Second), now.Add(time.Minute)); err == nil {
		t.Fatal("预留必须拒绝已过期心跳")
	}
	got, _ := r.Get("village")
	if got.ReservedPlayers != 0 {
		t.Fatal("拒绝后不得留下容量预留")
	}
}
