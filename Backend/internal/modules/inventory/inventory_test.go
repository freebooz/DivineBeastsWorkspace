package inventory

import (
	"context"
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
