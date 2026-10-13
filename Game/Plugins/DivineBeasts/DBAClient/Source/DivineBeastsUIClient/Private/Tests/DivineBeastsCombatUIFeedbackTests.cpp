#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#include <limits>

/**
 * 首个用例验证项目浮字DTO到中立UI请求；后续用例加载真实已保存技能条验证事件投影。
 * 全部输入为明确的开发测试夹具，不授予真实技能、不模拟服务端命中，不代替客户端视觉验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsCombatUIFeedbackTest,
    "DivineBeasts.UI.Combat.FeedbackMapping",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCombatUIFeedbackTest::RunTest(const FString&)
{
    FDivineBeastsCombatFeedbackInput Input;
    Input.EventId = FGuid::NewGuid();
    Input.TargetVisualKey = TEXT("Hero_Test_Target");
    Input.WorldLocation = FVector(100.0, 200.0, 300.0);
    Input.Magnitude = 35.0;

    FGamePlatformUIFeedbackRequest DamageRequest;
    TestTrue(TEXT("生命伤害转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, DamageRequest));

    FGamePlatformUIFeedbackRequest ShieldRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::ShieldDamage;
    Input.Magnitude = 12.0;
    TestTrue(TEXT("护盾吸收转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, ShieldRequest));
    TestNotEqual(TEXT("生命伤害与护盾吸收不能使用同一合并键"),
        DamageRequest.MergeKey, ShieldRequest.MergeKey);

    FGamePlatformUIFeedbackRequest HealingRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::Healing;
    Input.Magnitude = 23.0;
    TestTrue(TEXT("治疗转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, HealingRequest));
    TestNotEqual(TEXT("治疗与伤害不能合并"),
        DamageRequest.MergeKey, HealingRequest.MergeKey);
    TestNotEqual(TEXT("治疗与护盾不能合并"),
        HealingRequest.MergeKey, ShieldRequest.MergeKey);

    FGamePlatformUIFeedbackRequest DeathRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::Death;
    Input.Magnitude = 0.0;
    TestTrue(TEXT("死亡为独立非数值提示"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, DeathRequest));
    TestTrue(TEXT("死亡提示优先级应高于普通伤害"),
        DeathRequest.Priority > DamageRequest.Priority);

    Input.Kind = EDivineBeastsCombatFeedbackKind::Damage;
    Input.Magnitude = std::numeric_limits<double>::quiet_NaN();
    FGamePlatformUIFeedbackRequest InvalidRequest;
    TestFalse(TEXT("拒绝NaN数值"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, InvalidRequest));

    Input.Magnitude = 1.0e30;
    FGamePlatformUIFeedbackRequest HugeRequest;
    TestTrue(TEXT("过大数值可安全裁剪为UI格式化区间"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, HugeRequest));
    TestEqual(TEXT("显示数值不得溢出int32"),
        HugeRequest.NumericValue, 1000000000.0);

    Input.WorldLocation.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("拒绝非有限世界投影坐标"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, InvalidRequest));

    return true;
}

/**
 * 捕获“授权快照更新但真实AbilityIcon仍为空/上一角色图标”的回归。
 * 加载已保存Widget与两张真实纹理，直接消费只读槽位事件；不靠源码字符串或蓝图占位图判断成功。
 * 同时覆盖待确认、换角色及解绑；权限/冷却/成本/伤害仍由既有ViewModel与GAS拥有。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityIconProjectionTest,
    "DivineBeasts.UI.Combat.AbilityIconProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilityIconProjectionTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsAbilityBarPanel>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar.WBP_DBA_UI_AbilityBar_C"));
    if (!TestNotNull(TEXT("实际技能条类"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsAbilityBarPanel> Bar(NewObject<UDivineBeastsAbilityBarPanel>(GetTransientPackage(), Class));
    Bar->Initialize();
    auto* Slot = Cast<UGamePlatformSlotWidget>(Bar->GetWidgetFromName(TEXT("ActiveAbilitySlot1")));
    if (!TestNotNull(TEXT("真实主动槽1"), Slot)) return false;
    Slot->Initialize();
    auto* Icon = Cast<UImage>(Slot->GetWidgetFromName(TEXT("AbilityIcon")));
    if (!TestNotNull(TEXT("实际图标控件"), Icon)) return false;
    auto* Rat = LoadObject<UTexture2D>(nullptr,
        TEXT("/DBAHeroPack_Rat/UI/Portraits/T_DBA_Rat_Portrait.T_DBA_Rat_Portrait"));
    auto* Ox = LoadObject<UTexture2D>(nullptr,
        TEXT("/DBAHeroPack_Ox/UI/Portraits/T_DBA_Ox_Portrait.T_DBA_Ox_Portrait"));
    if (!TestNotNull(TEXT("已保存Rat纹理夹具"), Rat) || !TestNotNull(TEXT("已保存Ox纹理夹具"), Ox)) return false;
    FGamePlatformUISlotState State;
    State.SlotId = TEXT("Platform.Ability.Input.DivineBeasts.Slot1");
    State.ContentId = TEXT("Ability.Test.Fire");
    State.Icon = Rat;
    State.bPending = true;
    State.bEnabled = false;
    State.OverlayProgress = 0.25f;
    Bar->ApplyAbilitySlots({State});
    TestEqual(TEXT("待确认也显示该授权技能图标"), Icon->GetBrush().GetResourceObject(), static_cast<UObject*>(Rat));
    TestFalse(TEXT("投影不擅自启用待确认技能"), Slot->GetSlotState().bEnabled);
    State.Icon = Ox;
    State.OverlayProgress = 2.0f;
    Bar->ApplyAbilitySlots({State});
    TestEqual(TEXT("换角色不沿用旧图标"), Icon->GetBrush().GetResourceObject(), static_cast<UObject*>(Ox));
    Bar->ApplyAbilitySlots({});
    TestNull(TEXT("解绑清除实际画刷"), Icon->GetBrush().GetResourceObject());
    return true;
}
// 真实已保存技能条Widget的生命周期回归；控制UI事件输入，不授予或激活网络技能。
// 同一对象移除再加入后应继续显示异步快照，且重复刷新不能导致一次事件通知多次。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityBarReconstructTest,
    "DivineBeasts.UI.Combat.AbilityBarReconstruct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilityBarReconstructTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsAbilityBarPanel>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar.WBP_DBA_UI_AbilityBar_C"));
    if (!TestNotNull(TEXT("真实已保存技能条类"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsAbilityBarPanel> Widget(NewObject<UDivineBeastsAbilityBarPanel>(GetTransientPackage(), Class));
    Widget->Initialize();
    Widget->NativeConstruct();
    UDivineBeastsAbilityBarViewModel* Original = Widget->AbilityBarViewModel;
    if (!TestNotNull(TEXT("实际技能视图模型"), Original)) return false;
    Widget->NativeDestruct();
    Widget->NativeConstruct();
    TestEqual(TEXT("保留并复用同一视图模型"), Widget->AbilityBarViewModel.Get(), Original);
    Widget->RefreshAbilitySourceFromOwningPawn();
    Widget->RefreshAbilitySourceFromOwningPawn();
    FGamePlatformUISlotState Slot;
    Slot.SlotId = TEXT("Platform.Ability.Input.DivineBeasts.Primary");
    TArray<FGamePlatformUISlotState> Snapshot { Slot };
    const int32 Revision = Widget->GetPresentationRevision();
    Original->OnSlotsChanged().Broadcast(Snapshot);
    TestEqual(TEXT("复用后事件更新实际槽位"), Widget->GetAbilitySlotsView().Num(), 1);
    TestEqual(TEXT("一次视图事件只发布一次变化"), Widget->GetPresentationRevision(), Revision + 1);
    Widget->NativeDestruct();
    const int32 InactiveRevision = Widget->GetPresentationRevision();
    Original->OnSlotsChanged().Broadcast({});
    TestEqual(TEXT("失活后不再消费异步视图事件"), Widget->GetPresentationRevision(), InactiveRevision);
    return true;
}
#endif
