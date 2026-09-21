package playerdata

import (
	"context"
	"testing"
)

func TestUpdateProfileUsesOptimisticRevision(t *testing.T) {
	repo := NewMemoryRepository()
	repo.Seed(Profile{PlayerID: "player-1", DisplayName: "旧名称", Revision: 1, DataVersion: 1})
	service := NewService(repo)

	updated, err := service.UpdateDisplayName(context.Background(), "player-1", "新名称", 1)
	if err != nil {
		t.Fatalf("第一次更新失败: %v", err)
	}
	if updated.Revision != 2 || updated.DisplayName != "新名称" {
		t.Fatalf("更新结果不正确: %+v", updated)
	}

	if _, err := service.UpdateDisplayName(context.Background(), "player-1", "冲突名称", 1); err == nil {
		t.Fatal("旧Revision必须触发并发冲突")
	}
}

// TestGetProfileReturnsClone（读取玩家资料测试）验证PlayerData Service提供只读资料访问并避免切片别名泄漏。
func TestGetProfileReturnsClone(t *testing.T) {
	repo := NewMemoryRepository()
	repo.Seed(Profile{PlayerID: "p1", GameID: "divine-beasts", DisplayName: "玩家一", DataVersion: 1, Revision: 1, OwnedCharacterIDs: []string{"Character.Tiger.WhiteLord"}})
	service := NewService(repo)
	profile, err := service.GetProfile(context.Background(), "p1")
	if err != nil {
		t.Fatal(err)
	}
	profile.OwnedCharacterIDs[0] = "changed"
	again, err := service.GetProfile(context.Background(), "p1")
	if err != nil {
		t.Fatal(err)
	}
	if again.OwnedCharacterIDs[0] != "Character.Tiger.WhiteLord" {
		t.Fatal("GetProfile必须返回独立副本")
	}
}
