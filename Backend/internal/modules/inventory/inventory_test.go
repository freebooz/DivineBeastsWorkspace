package inventory

import (
	"context"
	"math"
	"testing"
)

func TestMemoryRepositoryIdempotencyAndRevision(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryRepository()
	const playerID = "player-test"

	if err := repo.SeedForTest(playerID, []ItemInstance{
		{
			ItemInstanceID: "item-a",
			ItemDefinitionID: "item.definition.a",
			Quantity: 2,
			ContainerID: DefaultContainerID,
			SlotIndex: 0,
			Revision: 1,
			InstanceState: "active",
			MaxStackSize: 20,
		},
	}); err != nil {
		t.Fatalf("SeedForTest失败: %v", err)
	}

	before, err := repo.GetSnapshot(ctx, playerID)
	if err != nil {
		t.Fatalf("GetSnapshot失败: %v", err)
	}

	command := MoveCommand{
		OperationID: "operation-move-1",
		ExpectedRevision: before.InventoryRevision,
		ItemInstanceID: "item-a",
		TargetContainerID: DefaultContainerID,
		TargetSlotIndex: 1,
	}

	first, err := repo.Move(ctx, playerID, command)
	if err != nil {
		t.Fatalf("首次Move失败: %v", err)
	}
	if first.Duplicate {
		t.Fatal("首次Move不应标记Duplicate")
	}
	if first.Snapshot.InventoryRevision != before.InventoryRevision+1 {
		t.Fatalf("Revision未递增: got=%d want=%d",
			first.Snapshot.InventoryRevision, before.InventoryRevision+1)
	}

	replayed, err := repo.Move(ctx, playerID, command)
	if err != nil {
		t.Fatalf("同OperationId同内容重放失败: %v", err)
	}
	if !replayed.Duplicate {
		t.Fatal("幂等重放必须标记Duplicate")
	}
	if replayed.Snapshot.InventoryRevision != first.Snapshot.InventoryRevision {
		t.Fatal("幂等重放不得再次推进Revision")
	}

	conflicting := command
	conflicting.TargetSlotIndex = 2
	if _, err = repo.Move(ctx, playerID, conflicting); err == nil {
		t.Fatal("同OperationId不同内容必须拒绝")
	}
}

func TestMemoryRepositorySplitMergeAndQuickbar(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryRepository()
	const playerID = "player-split"

	if err := repo.SeedForTest(playerID, []ItemInstance{
		{
			ItemInstanceID: "item-source",
			ItemDefinitionID: "item.definition.stack",
			Quantity: 10,
			ContainerID: DefaultContainerID,
			SlotIndex: 0,
			Revision: 1,
			InstanceState: "active",
			MaxStackSize: 20,
		},
	}); err != nil {
		t.Fatalf("SeedForTest失败: %v", err)
	}

	snapshot, err := repo.GetSnapshot(ctx, playerID)
	if err != nil {
		t.Fatal(err)
	}
	split, err := repo.Split(ctx, playerID, SplitCommand{
		OperationID: "operation-split-1",
		ExpectedRevision: snapshot.InventoryRevision,
		SourceItemInstanceID: "item-source",
		SplitQuantity: 4,
		TargetContainerID: DefaultContainerID,
		TargetSlotIndex: 1,
	})
	if err != nil {
		t.Fatalf("Split失败: %v", err)
	}
	if split.MovedQuantity != 4 || split.RemainingQuantity != 6 {
		t.Fatalf("Split数量错误: moved=%d remaining=%d",
			split.MovedQuantity, split.RemainingQuantity)
	}
	if len(split.Snapshot.Items) != 2 {
		t.Fatalf("Split后应有2个实例，实际%d", len(split.Snapshot.Items))
	}

	var splitItemID string
	for _, item := range split.Snapshot.Items {
		if item.ItemInstanceID != "item-source" {
			splitItemID = item.ItemInstanceID
		}
	}
	if splitItemID == "" {
		t.Fatal("未找到拆分生成实例")
	}

	quickbar, err := repo.SetQuickbar(ctx, playerID, QuickbarCommand{
		OperationID: "operation-quickbar-1",
		ExpectedRevision: split.Snapshot.InventoryRevision,
		SlotIndex: 0,
		ItemInstanceID: splitItemID,
	})
	if err != nil {
		t.Fatalf("SetQuickbar失败: %v", err)
	}
	if len(quickbar.Snapshot.Quickbar) != 1 {
		t.Fatal("快捷栏引用未建立")
	}

	merged, err := repo.Merge(ctx, playerID, MergeCommand{
		OperationID: "operation-merge-1",
		ExpectedRevision: quickbar.Snapshot.InventoryRevision,
		SourceItemInstanceID: splitItemID,
		TargetItemInstanceID: "item-source",
	})
	if err != nil {
		t.Fatalf("Merge失败: %v", err)
	}
	if len(merged.Snapshot.Items) != 1 {
		t.Fatal("Merge后应只剩一个实例")
	}
	if len(merged.Snapshot.Quickbar) != 1 ||
		merged.Snapshot.Quickbar[0].ItemInstanceID != "item-source" {
		t.Fatal("Merge必须把快捷栏引用原子重定向到目标实例")
	}
}


// testTrustedConsumeAuthorizer（可信消耗测试验证器）只用于本包测试，生产必须接真实游戏服会话绑定。
type testTrustedConsumeAuthorizer struct{ expectedPlayerID string }

func (a testTrustedConsumeAuthorizer) AuthorizeInventoryConsume(ctx context.Context, playerID string, cmd ConsumeCommand) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if playerID != a.expectedPlayerID || cmd.ReasonCode != "item_use" {
		return consumeNotAllowed()
	}
	return nil
}

// 测试可信服务边界、部分消耗、全量消耗快捷栏级联、幂等重放及同操作号异内容拒绝。
// 本测试不代表角色效果已经执行；游戏服必须先完成来源授权，效果提交还需事务Outbox闭环。
func TestTrustedConsumeAuthorityAndIdempotency(t *testing.T) {
	ctx := context.Background()
	repo := NewMemoryRepository()
	const playerID = "player-trusted-consume"
	const itemID = "consumable-potion"
	if err := repo.SeedForTest(playerID, []ItemInstance{{
		ItemInstanceID: itemID, ItemDefinitionID: "game.item.potion",
		Quantity: 5, ContainerID: DefaultContainerID, SlotIndex: 0,
		Revision: 1, InstanceState: "active", MaxStackSize: 10,
	}}); err != nil {
		t.Fatal(err)
	}
	before, err := repo.GetSnapshot(ctx, playerID)
	if err != nil {
		t.Fatal(err)
	}
	base := ConsumeCommand{
		OperationID: "event-consume-one", ExpectedRevision: before.InventoryRevision,
		ItemInstanceID: itemID, Quantity: 2, ReasonCode: "item_use",
	}
	// 默认服务不得因为HTTP路由或客户端猜测OperationId而消耗权威库存。
	if _, err := NewService(repo).ConsumeTrusted(ctx, playerID, base); err == nil {
		t.Fatal("未配置服务器验证器必须拒绝消耗")
	}
	afterReject, err := repo.GetSnapshot(ctx, playerID)
	if err != nil || afterReject.InventoryRevision != before.InventoryRevision {
		t.Fatalf("未授权消耗不得推进修订号: %+v %v", afterReject, err)
	}
	service := NewTrustedService(repo, testTrustedConsumeAuthorizer{expectedPlayerID: playerID})
	if _, err := service.ConsumeTrusted(ctx, "wrong-player", base); err == nil {
		t.Fatal("验证器不得授予错误玩家消耗权限")
	}
	invalidReason := base
	invalidReason.ReasonCode = "unverified-client-action"
	if _, err := service.ConsumeTrusted(ctx, playerID, invalidReason); err == nil {
		t.Fatal("验证器必须拒绝非法事件来源")
	}
	first, err := service.ConsumeTrusted(ctx, playerID, base)
	if err != nil || first.MovedQuantity != 2 || first.RemainingQuantity != 3 ||
		len(first.Snapshot.Items) != 1 || first.Snapshot.Items[0].Quantity != 3 {
		t.Fatalf("首次可信消耗结果错误: %+v %v", first, err)
	}
	repeated, err := service.ConsumeTrusted(ctx, playerID, base)
	if err != nil || !repeated.Duplicate || repeated.Snapshot.InventoryRevision != first.Snapshot.InventoryRevision {
		t.Fatalf("重复操作必须返回原始结果且不得二次扣除: %+v %v", repeated, err)
	}
	conflict := base
	conflict.Quantity = 1
	if _, err := service.ConsumeTrusted(ctx, playerID, conflict); err == nil {
		t.Fatal("同OperationId改变扣除数量必须拒绝")
	}
	quick, err := repo.SetQuickbar(ctx, playerID, QuickbarCommand{
		OperationID: "event-quickbar", ExpectedRevision: first.Snapshot.InventoryRevision,
		SlotIndex: 2, ItemInstanceID: itemID,
	})
	if err != nil || len(quick.Snapshot.Quickbar) != 1 {
		t.Fatalf("绑定快捷栏失败: %+v %v", quick, err)
	}
	full := ConsumeCommand{
		OperationID: "event-consume-two", ExpectedRevision: quick.Snapshot.InventoryRevision,
		ItemInstanceID: itemID, Quantity: 3, ReasonCode: "item_use",
	}
	result, err := service.ConsumeTrusted(ctx, playerID, full)
	if err != nil || result.MovedQuantity != 3 || result.RemainingQuantity != 0 ||
		len(result.Snapshot.Items) != 0 || len(result.Snapshot.Quickbar) != 0 {
		t.Fatalf("全量消耗必须原子移除背包和快捷栏引用: %+v %v", result, err)
	}
	old := full
	old.OperationID = "event-consume-stale"
	if _, err := service.ConsumeTrusted(ctx, playerID, old); err == nil {
		t.Fatal("物品已删除或修订过期时不能再消耗")
	}
}

// 内存仓储达到修订号上限时不能先扣除物品再报告失败；与PG事务回滚语义对齐。
func TestConsumeAtRevisionLimitIsAtomic(t *testing.T) {
	ctx := context.Background()
	const playerID = "player-overflow"
	repo := NewMemoryRepository()
	if err := repo.SeedForTest(playerID, []ItemInstance{{
		ItemInstanceID: "max-revision-item", ItemDefinitionID: "game.item.test",
		Quantity: 2, ContainerID: DefaultContainerID, SlotIndex: 0,
		Revision: 1, InstanceState: "active", MaxStackSize: 10,
	}}); err != nil {
		t.Fatal(err)
	}

	repo.mu.Lock()
	repo.stateLocked(playerID).revision = math.MaxInt64
	repo.mu.Unlock()
	_, err := repo.Consume(ctx, playerID, ConsumeCommand{
		OperationID: "max-revision-consume",
		ExpectedRevision: math.MaxInt64,
		ItemInstanceID: "max-revision-item",
		Quantity: 1,
		ReasonCode: "item_use",
	})
	if err == nil {
		t.Fatal("修订号不能递增时，消费必须失败")
	}
	snapshot, err := repo.GetSnapshot(ctx, playerID)
	if err != nil || snapshot.InventoryRevision != math.MaxInt64 ||
		len(snapshot.Items) != 1 || snapshot.Items[0].Quantity != 2 {
		t.Fatalf("失败事务不得改动库存和修订号: %+v %v", snapshot, err)
	}
}

