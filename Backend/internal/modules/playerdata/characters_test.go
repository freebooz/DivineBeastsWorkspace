package playerdata

import (
	"context"
	"testing"

	"divinebeasts/backend/internal/platform/apperror"
)

func requireCharacterCode(t *testing.T, err error, code string) {
	t.Helper()
	app, ok := err.(*apperror.Error)
	if !ok || app.Code != code {
		t.Fatalf("错误码应为%s，实际=%v", code, err)
	}
}

// TestCharacterCreateAndSelectionFlow（角色创建与选择主流程）验证创建幂等、资料拥有关系和权威选择Revision。
func TestCharacterCreateAndSelectionFlow(t *testing.T) {
	repo := NewMemoryRepository()
	service := NewService(repo)
	ctx := context.Background()

	if err := service.EnsureProfile(ctx, "player-a", "divine-beasts"); err != nil {
		t.Fatal(err)
	}
	items, err := service.ListCharacters(ctx, "player-a")
	if err != nil || len(items) != 0 {
		t.Fatalf("新玩家角色列表应为空: %+v %v", items, err)
	}

	first, err := service.CreateCharacter(
		ctx,
		"player-a",
		"create-1",
		"Hero.Zodiac.Tiger",
		"  白君  ",
		map[string]string{"Skin": "Default"},
	)
	if err != nil {
		t.Fatal(err)
	}
	if first.CharacterName != "白君" ||
		first.CharacterRevision != 1 ||
		first.Status != CharacterStatusActive ||
		first.OnboardingState != OnboardingTutorialRequired {
		t.Fatalf("创建角色结果错误: %+v", first)
	}

	replay, err := service.CreateCharacter(
		ctx,
		"player-a",
		"create-1",
		"Hero.Zodiac.Tiger",
		"白君",
		map[string]string{"Skin": "Default"},
	)
	if err != nil || replay.CharacterID != first.CharacterID {
		t.Fatalf("同键同内容必须重放原角色: %+v %v", replay, err)
	}
	_, err = service.CreateCharacter(
		ctx,
		"player-a",
		"create-1",
		"Hero.Zodiac.Dragon",
		"苍龙",
		nil,
	)
	requireCharacterCode(t, err, "CHARACTER_CONFLICT")

	profile, err := service.GetProfile(ctx, "player-a")
	if err != nil {
		t.Fatal(err)
	}
	if profile.Revision != 2 ||
		len(profile.OwnedCharacterIDs) != 1 ||
		profile.OwnedCharacterIDs[0] != first.CharacterID {
		t.Fatalf("创建角色没有原子更新玩家资料: %+v", profile)
	}

	selection, err := service.SelectCharacter(
		ctx, "player-a", "select-1", first.CharacterID, first.CharacterRevision)
	if err != nil {
		t.Fatal(err)
	}
	if selection.ProfileRevision != 3 ||
		selection.Character.CharacterID != first.CharacterID {
		t.Fatalf("角色选择结果错误: %+v", selection)
	}

	replaySelection, err := service.SelectCharacter(
		ctx, "player-a", "select-1", first.CharacterID, first.CharacterRevision)
	if err != nil ||
		replaySelection.ProfileRevision != selection.ProfileRevision {
		t.Fatalf("选择重放必须返回第一次结果: %+v %v", replaySelection, err)
	}
	profile, err = service.GetProfile(ctx, "player-a")
	if err != nil || profile.SelectedCharacterID != first.CharacterID || profile.Revision != 3 {
		t.Fatalf("权威选择没有更新Profile或重复推进Revision: %+v %v", profile, err)
	}
}

// TestCharacterOwnershipIsolation（角色归属隔离）验证玩家不能选择其他玩家的持久角色。
func TestCharacterOwnershipIsolation(t *testing.T) {
	repo := NewMemoryRepository()
	service := NewService(repo)
	ctx := context.Background()

	for _, playerID := range []string{"player-a", "player-b"} {
		if err := service.EnsureProfile(ctx, playerID, "divine-beasts"); err != nil {
			t.Fatal(err)
		}
	}
	character, err := service.CreateCharacter(
		ctx, "player-a", "create-a", "Hero.Zodiac.Rat", "影牙", nil)
	if err != nil {
		t.Fatal(err)
	}

	_, err = service.SelectCharacter(
		ctx, "player-b", "select-b", character.CharacterID, character.CharacterRevision)
	requireCharacterCode(t, err, "CHARACTER_NOT_FOUND")
	items, err := service.ListCharacters(ctx, "player-b")
	if err != nil || len(items) != 0 {
		t.Fatalf("其他玩家不能看到角色: %+v %v", items, err)
	}
}

// TestCharacterValidation（角色输入校验）验证非法创建/选择请求不会进入持久状态。
func TestCharacterValidation(t *testing.T) {
	repo := NewMemoryRepository()
	service := NewService(repo)
	ctx := context.Background()
	if err := service.EnsureProfile(ctx, "player-a", "divine-beasts"); err != nil {
		t.Fatal(err)
	}

	_, err := service.CreateCharacter(ctx, "player-a", "", "Hero", "名字", nil)
	requireCharacterCode(t, err, "INVALID_REQUEST")
	_, err = service.CreateCharacter(ctx, "player-a", "key", "", "名字", nil)
	requireCharacterCode(t, err, "INVALID_REQUEST")

	character, err := service.CreateCharacter(ctx, "player-a", "key", "Hero.Zodiac.Ox", "玄角", nil)
	if err != nil {
		t.Fatal(err)
	}
	_, err = service.SelectCharacter(ctx, "player-a", "", character.CharacterID, 1)
	requireCharacterCode(t, err, "INVALID_REQUEST")
	_, err = service.SelectCharacter(ctx, "player-a", "select", character.CharacterID, 2)
	requireCharacterCode(t, err, "CHARACTER_CONFLICT")
}
