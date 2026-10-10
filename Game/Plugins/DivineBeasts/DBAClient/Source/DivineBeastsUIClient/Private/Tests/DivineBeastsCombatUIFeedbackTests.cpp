#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

/**
 * 只验证项目客户端浮字DTO转换为平台UI请求的纯数据逻辑；
 * 不生成Widget、不模拟服务端命中，也不将测试源码冒充可视化验收。
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
