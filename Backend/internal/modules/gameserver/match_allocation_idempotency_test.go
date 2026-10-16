// 本文件验证MainArena分配意图重放；恢复未知结果不得为同一MatchID额外占用第二台服务器。
package gameserver

import (
	"context"
	"testing"
)

// TestMainArenaAllocationRetryKeepsOneInstance覆盖同比赛重试和人数异参冲突，不影响普通世界容量预留语义。
func TestMainArenaAllocationRetryKeepsOneInstance(t *testing.T) {
	registry := NewRegistry()
	for _, id := range []string{"arena-one", "arena-two"} {
		registry.Register(Instance{ID: id, RoleID: RoleMainArena, ExperienceID: "Experience.MainArena.Main", RegionID: "region", Status: StatusReady, Capacity: 10})
	}
	allocator := NewRegistryAllocator(registry)
	request := AllocationRequest{RoleID: RoleMainArena, ExperienceID: "Experience.MainArena.Main", RegionID: "region", RequiredCapacity: 4, MatchID: "one-match"}
	first, err := allocator.Allocate(context.Background(), request)
	if err != nil {
		t.Fatal(err)
	}
	second, err := allocator.Allocate(context.Background(), request)
	if err != nil {
		t.Fatal(err)
	}
	if first.ID != second.ID {
		t.Fatal("同比赛分配重试占用了第二个实例")
	}
	request.RequiredCapacity = 6
	if _, err := allocator.Allocate(context.Background(), request); err == nil {
		t.Fatal("同比赛不能用异参改变已分配容量")
	}
}
