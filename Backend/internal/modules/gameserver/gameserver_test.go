package gameserver

import (
	"testing"
	"time"
)

func TestAllocateChoosesReadyServerByRoleRegionAndCapacity(t *testing.T) {
	registry := NewRegistry()
	now := time.Date(2026, 9, 16, 8, 0, 0, 0, time.UTC)
	registry.Register(Instance{ID: "arena-small", RoleID: RoleMainArena, RegionID: "us-west", Capacity: 4, Status: StatusReady, LastHeartbeat: now})
	registry.Register(Instance{ID: "arena-large", RoleID: RoleMainArena, RegionID: "us-west", Capacity: 10, Status: StatusReady, LastHeartbeat: now})

	allocated, err := registry.Allocate(AllocationRequest{RoleID: RoleMainArena, RegionID: "us-west", RequiredCapacity: 10, MatchID: "match-1"})
	if err != nil {
		t.Fatalf("分配失败: %v", err)
	}
	if allocated.ID != "arena-large" || allocated.CurrentMatchID != "match-1" || allocated.Status != StatusActive {
		t.Fatalf("分配结果不正确: %+v", allocated)
	}
}

func TestDrainingServerCannotBeAllocated(t *testing.T) {
	registry := NewRegistry()
	registry.Register(Instance{ID: "arena-1", RoleID: RoleMainArena, RegionID: "us-west", Capacity: 10, Status: StatusDraining})
	if _, err := registry.Allocate(AllocationRequest{RoleID: RoleMainArena, RegionID: "us-west", RequiredCapacity: 2}); err == nil {
		t.Fatal("Draining服务器不能被分配")
	}
}

// TestKnownServerRoles（正式服务器角色校验测试）确保三类角色可注册，历史/非法角色被拒绝。
func TestKnownServerRoles(t *testing.T) {
	for _, role := range []string{RoleOpenWorld, RoleVillage, RoleMainArena} {
		if !IsKnownRole(role) {
			t.Fatalf("正式服务器角色应被识别: %s", role)
		}
	}
	if IsKnownRole("GameServer.Role.Training") {
		t.Fatal("Training是Village Experience，不得成为独立Server Role")
	}
}
