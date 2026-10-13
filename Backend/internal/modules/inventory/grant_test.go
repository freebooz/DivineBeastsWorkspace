package inventory

// 可信拾取/任务奖励测试：只测试后端事件、授权、幂等、槽位原子计划；
// 不将内存仓储模拟当作实际Dedicated Server联机或生产PostgreSQL迁移验收。

import (
	"context"
	"testing"
)

type testGrantAuthorizer struct {
	playerID string
	rule     GrantRule
}

func (a testGrantAuthorizer) AuthorizeInventoryGrant(ctx context.Context, playerID string, cmd GrantCommand) (GrantRule, error) {
	if err := ctx.Err(); err != nil {
		return GrantRule{}, err
	}
	// 测试替身：验证玩家、服务器Boot、来源事件及可信物品定义。
	// 生产实现还必须检查游戏服务器Registry、会话归属和来源事件真实领取资格。
	if playerID != a.playerID || cmd.GameServerID != "world-village-1" ||
		cmd.ServerBootID != "boot-20261013" || cmd.SourceEventID == "" ||
		cmd.ItemDefinitionID != "item.herb.peach" ||
		(cmd.ReasonCode != "pickup" && cmd.ReasonCode != "quest_reward") {
		return GrantRule{}, grantNotAllowed()
	}
	return a.rule, nil
}

func trustedGrant(playerID, eventID string, revision int64, quantity int32) GrantCommand {
	return GrantCommand{
		OperationID:      GrantOperationID(playerID, eventID),
		ExpectedRevision: revision,
		GameServerID:     "world-village-1",
		ServerBootID:     "boot-20261013",
		SourceEventID:    eventID,
		ReasonCode:       "pickup",
		ItemDefinitionID: "item.herb.peach",
		Quantity:         quantity,
	}
}

func TestTrustedGrantAtomicStackPlacementAndReplay(t *testing.T) {
	ctx := context.Background()
	const playerID = "player-gatherer"
	repo := NewMemoryRepository()
	if err := repo.SeedForTest(playerID, []ItemInstance{{
		ItemInstanceID: "existing-herb",
		ItemDefinitionID: "item.herb.peach", Quantity: 3,
		ContainerID: DefaultContainerID, SlotIndex: 3,
		Revision: 1, InstanceState: "active", MaxStackSize: 5,
	}}); err != nil {
		t.Fatal(err)
	}
	before, err := repo.GetSnapshot(ctx, playerID)
	if err != nil {
		t.Fatal(err)
	}
	cmd := trustedGrant(playerID, "drop-0031", before.InventoryRevision, 9)
	if _, err = NewService(repo).GrantTrusted(ctx, playerID, cmd); err == nil {
		t.Fatal("普通背包服务必须拒绝授予")
	}
	if got, _ := repo.GetSnapshot(ctx, playerID); got.InventoryRevision != before.InventoryRevision {
		t.Fatal("越权失败不得改动版本")
	}
	service := NewTrustedGrantService(repo, testGrantAuthorizer{playerID: playerID, rule: GrantRule{MaxStackSize: 5}}, nil)
	for _, broken := range []GrantCommand{
		func() GrantCommand { x := cmd; x.OperationID = "different-id"; return x }(),
		func() GrantCommand { x := cmd; x.ServerBootID = "stale-boot"; return x }(),
		func() GrantCommand { x := cmd; x.ReasonCode = "client_forged"; return x }(),
		func() GrantCommand { x := cmd; x.Quantity = -1; return x }(),
	} {
		if _, err = service.GrantTrusted(ctx, playerID, broken); err == nil {
			t.Fatalf("伪造/非法奖励必须拒绝: %+v", broken)
		}
	}
	first, err := service.GrantTrusted(ctx, playerID, cmd)
	if err != nil {
		t.Fatal(err)
	}
	if first.Duplicate || first.MovedQuantity != 9 ||
		first.Snapshot.InventoryRevision != before.InventoryRevision+1 ||
		len(first.Snapshot.Items) != 3 {
		t.Fatalf("授予数量和版本不正确: %+v", first)
	}
	expected := map[int32]int32{0: 5, 1: 2, 3: 5}
	for _, item := range first.Snapshot.Items {
		if got, exists := expected[item.SlotIndex]; !exists || got != item.Quantity {
			t.Fatalf("先填堆叠再按空格升序放置不一致: %+v", item)
		}
	}
	replayed, err := service.GrantTrusted(ctx, playerID, cmd)
	if err != nil || !replayed.Duplicate ||
		replayed.Snapshot.InventoryRevision != first.Snapshot.InventoryRevision ||
		len(replayed.Snapshot.Items) != 3 {
		t.Fatalf("同事件同OperationId必须只发奖一次: %+v %v", replayed, err)
	}
	altered := cmd
	altered.Quantity = 1
	if _, err = service.GrantTrusted(ctx, playerID, altered); err == nil {
		t.Fatal("同事件更改数量不得通过幂等保护")
	}
	ruleChanged := NewTrustedGrantService(repo,
		testGrantAuthorizer{playerID: playerID, rule: GrantRule{MaxStackSize: 10}}, nil)
	if _, err = ruleChanged.GrantTrusted(ctx, playerID, cmd); err == nil {
		t.Fatal("同事件改变权威堆叠规则必须作为冲突拒绝")
	}
}

func TestTrustedGrantFullInventoryDoesNotPartiallyFill(t *testing.T) {
	ctx := context.Background()
	const playerID = "player-full"
	repo := NewMemoryRepository()
	seed := make([]ItemInstance, 0, DefaultContainerCapacity)
	for i := int32(0); i < DefaultContainerCapacity; i++ {
		seed = append(seed, ItemInstance{
			ItemInstanceID: deterministicGrantID(playerID, "seed", i),
			ItemDefinitionID: "item.herb.peach",
			Quantity: 4,
			ContainerID: DefaultContainerID,
			SlotIndex: i, Revision: 1, InstanceState: "active", MaxStackSize: 5,
		})
	}
	if err := repo.SeedForTest(playerID, seed); err != nil {
		t.Fatal(err)
	}
	before, _ := repo.GetSnapshot(ctx, playerID)
	service := NewTrustedGrantService(repo, testGrantAuthorizer{playerID: playerID, rule: GrantRule{MaxStackSize: 5}}, nil)
	cmd := trustedGrant(playerID, "drop-too-large", before.InventoryRevision, DefaultContainerCapacity+1)
	if _, err := service.GrantTrusted(ctx, playerID, cmd); err == nil {
		t.Fatal("超过所有可用堆叠空间时必须全部拒绝")
	}
	after, _ := repo.GetSnapshot(ctx, playerID)
	if after.InventoryRevision != before.InventoryRevision || len(after.Items) != len(before.Items) {
		t.Fatal("背包空间不足不得推进修订或部分写入物品")
	}
	for _, item := range after.Items {
		if item.Quantity != 4 {
			t.Fatalf("授予失败不能悄悄先补入部分堆叠: %+v", item)
		}
	}
}

func TestTrustedGrantAcrossContainersAndRevision(t *testing.T) {
	playerID := "player-multi-container"
	command := trustedGrant(playerID, "reward-from-quest-7", 8, 4)
	command.ReasonCode = "quest_reward"
	before := Snapshot{
		InventoryRevision: 8,
		Containers: []ContainerSnapshot{
			{ContainerID: "store", Capacity: 2},
			{ContainerID: DefaultContainerID, Capacity: 1},
		},
		Items: []ItemInstance{{
			ItemInstanceID: "unrelated", ItemDefinitionID: "item.other",
			Quantity: 1, ContainerID: DefaultContainerID, SlotIndex: 0,
			Revision: 1, InstanceState: "active", MaxStackSize: 1,
		}},
	}
	plan, err := PlanTrustedGrant(playerID, before, command, GrantRule{MaxStackSize: 3})
	if err != nil || len(plan.Creates) != 2 || len(plan.Updates) != 0 {
		t.Fatalf("多容器首次放置计划错误: %+v %v", plan, err)
	}
	if plan.Creates[0].ContainerID != "store" || plan.Creates[0].Quantity != 3 ||
		plan.Creates[1].SlotIndex != 1 || plan.Creates[1].Quantity != 1 {
		t.Fatalf("容器和槽位稳定排序或分堆错误: %+v", plan)
	}
	if len(before.Items) != 1 {
		t.Fatal("纯计划不能修改入参Snapshot")
	}
	invalid := before
	invalid.Items = []ItemInstance{{
		ItemInstanceID: "illegal", ItemDefinitionID: "item.other",
		Quantity: 1, ContainerID: DefaultContainerID, SlotIndex: 5,
		Revision: 1, InstanceState: "active", MaxStackSize: 1,
	}}
	if _, err = PlanTrustedGrant(playerID, invalid, command, GrantRule{MaxStackSize: 3}); err == nil {
		t.Fatal("已有物品超出容器容量必须拒绝计划")
	}
	if GrantOperationID("player-a", "event-one") == GrantOperationID("player-b", "event-one") ||
		GrantOperationID("player-a", "event-one") != GrantOperationID("player-a", "event-one") {
		t.Fatal("来源事件操作号必须稳定且按玩家隔离")
	}
}
