package inventory

// 使用物品的正式授权测试：验证当前GameServer Boot和玩家绑定、事件数据、
// 物品权威规则与确定性OperationId；测试中只模拟服务器已记录的可信证据。

import (
	"context"
	"errors"
	"testing"
)

type fakeConsumeEvidenceSource struct{ evidence ConsumeEvidence }

func (s fakeConsumeEvidenceSource) FindConsumeEvidence(_ context.Context, eventID string) (ConsumeEvidence, error) {
	if s.evidence.SourceEventID != eventID {
		return ConsumeEvidence{}, errors.New("可信消耗事件不存在")
	}
	return s.evidence, nil
}

func TestVerifiedConsumeTrustAndIdempotentEffectRecord(t *testing.T) {
	const playerID = "player-consume-verified"
	const itemID = "item-potion-1"
	ctx := context.Background()
	repo := NewMemoryRepository()
	if err := repo.SeedForTest(playerID, []ItemInstance{{
		ItemInstanceID: itemID, ItemDefinitionID: "item.consumable.herbal_potion",
		Quantity: 3, ContainerID: DefaultContainerID, SlotIndex: 0,
		Revision: 1, InstanceState: "active", MaxStackSize: 10,
	}}); err != nil {
		t.Fatal(err)
	}
	before, _ := repo.GetSnapshot(ctx, playerID)
	cmd := ConsumeCommand{
		OperationID:      ConsumeOperationID(playerID, "use-event-1"),
		ExpectedRevision: before.InventoryRevision, SourceEventID: "use-event-1",
		ItemInstanceID: itemID, ItemDefinitionID: "item.consumable.herbal_potion",
		Quantity: 2, ReasonCode: "item_use", GameServerID: "world-village-1",
		ServerBootID: "boot-20261013",
	}
	evidence := ConsumeEvidence{
		SourceEventID: cmd.SourceEventID, GameServerID: cmd.GameServerID,
		ServerBootID: cmd.ServerBootID, PlayerID: playerID,
		ItemDefinitionID: cmd.ItemDefinitionID, ItemInstanceID: itemID,
		Quantity: cmd.Quantity, ReasonCode: cmd.ReasonCode,
	}
	catalog, err := NewStaticItemPolicyCatalog([]ItemPolicy{{
		ItemDefinitionID: cmd.ItemDefinitionID, MaxStackSize: 10,
		CanConsume: true, AllowedGrantReasons: []string{"quest_reward"},
	}})
	if err != nil {
		t.Fatal(err)
	}
	authorizer := &VerifiedConsumeAuthorizer{
		Servers:  fakeVerifiedServerPlayer{playerID, cmd.GameServerID, cmd.ServerBootID},
		Evidence: fakeConsumeEvidenceSource{evidence}, Catalog: catalog,
	}
	service := NewTrustedService(repo, authorizer)
	for _, tampered := range []ConsumeCommand{
		func() ConsumeCommand { x := cmd; x.ItemInstanceID = "other-item"; return x }(),
		func() ConsumeCommand { x := cmd; x.Quantity = 1; return x }(),
		func() ConsumeCommand { x := cmd; x.GameServerID = "foreign-world"; return x }(),
		func() ConsumeCommand { x := cmd; x.ServerBootID = "old-boot"; return x }(),
		func() ConsumeCommand { x := cmd; x.OperationID = "random"; return x }(),
		func() ConsumeCommand { x := cmd; x.ItemDefinitionID = "item.malicious"; return x }(),
	} {
		if _, err := service.ConsumeTrusted(ctx, playerID, tampered); err == nil {
			t.Fatalf("伪造消耗必须拒绝: %+v", tampered)
		}
	}
	result, err := service.ConsumeTrusted(ctx, playerID, cmd)
	if err != nil || result.RemainingQuantity != 1 || result.MovedQuantity != 2 {
		t.Fatalf("已认证使用事件应准确扣除: %+v %v", result, err)
	}
	replay, err := service.ConsumeTrusted(ctx, playerID, cmd)
	if err != nil || !replay.Duplicate ||
		replay.Snapshot.InventoryRevision != result.Snapshot.InventoryRevision {
		t.Fatalf("同使用事件重放不得二次扣除: %+v %v", replay, err)
	}
	// 切换为不可使用的后端物品目录时，旧操作也不能绕过正式授权器重放。
	notConsumable, _ := NewStaticItemPolicyCatalog([]ItemPolicy{{
		ItemDefinitionID: cmd.ItemDefinitionID, MaxStackSize: 10,
		CanConsume: false, AllowedGrantReasons: []string{"quest_reward"},
	}})
	authorizer.Catalog = notConsumable
	if _, err := service.ConsumeTrusted(ctx, playerID, cmd); err == nil {
		t.Fatal("授权已撤销时不得无条件允许重复消费")
	}
}
