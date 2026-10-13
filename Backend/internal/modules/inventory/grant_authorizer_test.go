package inventory

// 授权器测试：伪造玩家、旧Boot、错误事由、假奖励或假目录一律拒绝。

import (
	"context"
	"errors"
	"os"
	"strings"
	"testing"
)

type fakeVerifiedServerPlayer struct{ playerID, serverID, bootID string }

func (s fakeVerifiedServerPlayer) VerifyServerPlayer(_ context.Context, serverID, bootID, playerID string) error {
	if s.playerID != playerID || s.serverID != serverID || s.bootID != bootID {
		return errors.New("玩家或服务器会话不属于此实例")
	}
	return nil
}

type fakeRewardEventStore struct{ evidence GrantEvidence }

func (s fakeRewardEventStore) FindGrantEvidence(_ context.Context, eventID string) (GrantEvidence, error) {
	if s.evidence.SourceEventID != eventID {
		return GrantEvidence{}, errors.New("奖励事件不存在")
	}
	return s.evidence, nil
}

func TestVerifiedGrantAuthorizerChecksAllEvidence(t *testing.T) {
	ctx := context.Background()
	const playerID = "player-verified"
	cmd := trustedGrant(playerID, "drop-real-1", 1, 7)
	evidence := GrantEvidence{
		SourceEventID: cmd.SourceEventID, GameServerID: cmd.GameServerID,
		ServerBootID: cmd.ServerBootID, PlayerID: playerID,
		ReasonCode: "pickup", ItemDefinitionID: cmd.ItemDefinitionID, Quantity: cmd.Quantity,
	}
	catalog, err := NewStaticItemPolicyCatalog([]ItemPolicy{{
		ItemDefinitionID: cmd.ItemDefinitionID, MaxStackSize: 20,
		AllowedGrantReasons: []string{"pickup", "quest_reward"},
	}})
	if err != nil {
		t.Fatal(err)
	}
	if _, err := NewVerifiedGrantAuthorizer(nil, fakeRewardEventStore{evidence}, catalog); err == nil {
		t.Fatal("缺少游戏服务器与玩家绑定的验证器必须拒绝装配")
	}
	base := &VerifiedGrantAuthorizer{
		Servers:  fakeVerifiedServerPlayer{playerID, cmd.GameServerID, cmd.ServerBootID},
		Evidence: fakeRewardEventStore{evidence}, Catalog: catalog,
	}
	rule, err := base.AuthorizeInventoryGrant(ctx, playerID, cmd)
	if err != nil || rule.MaxStackSize != 20 {
		t.Fatalf("完整证据应授予权威目录堆叠上限: %+v %v", rule, err)
	}
	for _, changed := range []GrantCommand{
		func() GrantCommand { x := cmd; x.ItemDefinitionID = "item.forged"; return x }(),
		func() GrantCommand { x := cmd; x.Quantity = 8; return x }(),
		func() GrantCommand { x := cmd; x.ServerBootID = "boot-stale"; return x }(),
		func() GrantCommand { x := cmd; x.SourceEventID = "event-forged"; return x }(),
		func() GrantCommand { x := cmd; x.ReasonCode = "quest_reward"; return x }(),
	} {
		if _, err := base.AuthorizeInventoryGrant(ctx, playerID, changed); err == nil {
			t.Fatalf("来源事件逐字段比对失败: %+v", changed)
		}
	}
	if _, err := base.AuthorizeInventoryGrant(ctx, "other-account", cmd); err == nil {
		t.Fatal("玩家不能领取他人的掉落")
	}
	questOnly, err := NewStaticItemPolicyCatalog([]ItemPolicy{{
		ItemDefinitionID: cmd.ItemDefinitionID, MaxStackSize: 1,
		AllowedGrantReasons: []string{"quest_reward"},
	}})
	if err != nil {
		t.Fatal(err)
	}
	base.Catalog = questOnly
	if _, err := base.AuthorizeInventoryGrant(ctx, playerID, cmd); err == nil {
		t.Fatal("物品目录不能允许未批准的来源类型")
	}
}

func TestDeployedSSLMItemPolicyCatalog(t *testing.T) {
	// 用真实仓库的受控后端配置进行测试；配置不存在时失败，不自行创造默认奖励。
	file, err := os.Open("../../../config/inventory_item_policies_v1.json")
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	catalog, err := LoadItemPolicyCatalog(file)
	if err != nil {
		t.Fatal(err)
	}
	for _, definitionID := range []string{
		"item.herb.peach", "item.herb.mugwort", "item.material.lotus",
		"item.material.stone", "item.material.peachwood",
		"item.consumable.herbal_potion", "item.quest.village_badge",
		"item.material.silkworm_silk",
	} {
		policy, err := catalog.FindItemPolicy(context.Background(), definitionID)
		if err != nil || policy.ItemDefinitionID != definitionID || policy.MaxStackSize < 1 {
			t.Fatalf("服务端权威物品目录缺少必要定义: %s %+v %v", definitionID, policy, err)
		}
	}
	potion, _ := catalog.FindItemPolicy(context.Background(), "item.consumable.herbal_potion")
	if !potion.CanConsume {
		t.Fatal("草本药露应有服务端可使用策略，但不得直接在客户端扣除")
	}
	badge, _ := catalog.FindItemPolicy(context.Background(), "item.quest.village_badge")
	if badge.CanConsume || badge.MaxStackSize != 1 {
		t.Fatal("新手村徽记不可作为消耗品堆叠")
	}
}

func TestItemPolicyCatalogRejectsMalformedOrForgedFiles(t *testing.T) {
	invalid := []string{
		`{"schemaVersion":2,"gameId":"divine","items":[{"itemDefinitionId":"x","maxStackSize":1,"allowedGrantReasons":["pickup"]}]}`,
		`{"schemaVersion":1,"gameId":"divine","unknown":"forged","items":[{"itemDefinitionId":"x","maxStackSize":1,"allowedGrantReasons":["pickup"]}]}`,
		`{"schemaVersion":1,"gameId":"divine","items":[{"itemDefinitionId":"x","maxStackSize":0,"allowedGrantReasons":["pickup"]}]}`,
		`{"schemaVersion":1,"gameId":"divine","items":[{"itemDefinitionId":"x","maxStackSize":1,"allowedGrantReasons":["admin_backdoor"]}]}`,
		`{"schemaVersion":1,"gameId":"divine","items":[{"itemDefinitionId":"x","maxStackSize":1,"allowedGrantReasons":["pickup"]},{"itemDefinitionId":"x","maxStackSize":1,"allowedGrantReasons":["pickup"]}]}`,
	}
	for _, raw := range invalid {
		if _, err := LoadItemPolicyCatalog(strings.NewReader(raw)); err == nil {
			t.Fatalf("非法规则目录不得加载: %.120s", raw)
		}
	}
	valid := `{"schemaVersion":1,"gameId":"DivineBeastsArena","items":[{"itemDefinitionId":"item.herb.peach","maxStackSize":20,"allowedGrantReasons":["pickup","quest_reward"]}]}`
	catalog, err := LoadItemPolicyCatalog(strings.NewReader(valid))
	if err != nil {
		t.Fatal(err)
	}
	policy, err := catalog.FindItemPolicy(context.Background(), "item.herb.peach")
	if err != nil || policy.MaxStackSize != 20 {
		t.Fatalf("有效后端物品定义未加载: %+v %v", policy, err)
	}
	policy.AllowedGrantReasons[0] = "client_forged"
	reloaded, err := catalog.FindItemPolicy(context.Background(), "item.herb.peach")
	if err != nil || reloaded.AllowedGrantReasons[0] != "pickup" {
		t.Fatal("不可变后端策略不得被调用方借返回切片覆盖")
	}
}
