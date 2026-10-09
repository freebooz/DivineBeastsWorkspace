#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/GamePlatformCastProgressWidget.h"
#include "Components/GamePlatformTargetFrameWidget.h"
#include "Components/GamePlatformCombatAlertWidget.h"
#include "Components/GamePlatformStatusEffectTrayWidget.h"

/**
 * AAA战斗展示基础契约回归：
 * 仅校验平台纯数据变换与错误输入过滤，不创建真实Actor、不模拟客户端有权看到的敌方状态。
 * 真实GAS（能力系统）事件、复制权限和Monolith蓝图仍应独立集成验证。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUICombatPresentationTest,
    "GamePlatform.UI.CombatPresentation.SnapshotValidation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUICombatPresentationTest::RunTest(const FString&)
{
    // 施法数据：来自已经通过来源权限过滤的战斗客户端状态。
    FGamePlatformUICastProgressState CastState;
    CastState.SourceScopeId = FGuid::NewGuid();
    CastState.CastInstanceId = TEXT("Cast.Instance.7");
    CastState.TotalSeconds = 5.0;
    CastState.RemainingSeconds = 2.5;
    CastState.Revision = 1;
    CastState.bActive = true;
    TestTrue(TEXT("合法施法快照可使用"),
        FGamePlatformUICastProgressPresentation::IsValidSnapshot(CastState));
    TestTrue(TEXT("合法施法进度50%"),
        FMath::IsNearlyEqual(
            FGamePlatformUICastProgressPresentation::GetNormalizedProgress(CastState),
            0.5f));
    CastState.RemainingSeconds = 6.0;
    TestFalse(TEXT("剩余时间超出总时长需拒绝"),
        FGamePlatformUICastProgressPresentation::IsValidSnapshot(CastState));
    CastState.RemainingSeconds = 2.5;
    CastState.TotalSeconds = 0.0;
    TestTrue(TEXT("未知总时长可使用不定进度方式显示"),
        FGamePlatformUICastProgressPresentation::IsValidSnapshot(CastState));
    TestTrue(TEXT("未知总时长不得伪造百分比"),
        FMath::IsNearlyZero(
            FGamePlatformUICastProgressPresentation::GetNormalizedProgress(CastState)));

    // 目标数据：不具备观察者权限时不可展示旧生命和身份。
    FGamePlatformUITargetFrameState TargetState;
    TargetState.SourceScopeId = FGuid::NewGuid();
    TargetState.TargetDisplayId = TEXT("VisibleTarget.Instance.2");
    TargetState.Revision = 2;
    TargetState.bVisible = true;
    TargetState.Health.CurrentValue = 55.0;
    TargetState.Health.MaximumValue = 100.0;
    TestTrue(TEXT("已授权目标状态合法"),
        FGamePlatformUITargetFramePresentation::IsValidSnapshot(TargetState));
    TargetState.Health.CurrentValue = -4.0;
    TestFalse(TEXT("非法负生命不可进入显示投影"),
        FGamePlatformUITargetFramePresentation::IsValidSnapshot(TargetState));
    TargetState.bVisible = false;
    TestTrue(TEXT("隐藏目标无需再次验证不可见的资源字段"),
        FGamePlatformUITargetFramePresentation::IsValidSnapshot(TargetState));

    // 关键战斗警告由授权域发出，平台只验证输入而不预测机制。
    FGamePlatformUICombatAlertState AlertState;
    AlertState.SourceScopeId = FGuid::NewGuid();
    AlertState.AlertInstanceId = TEXT("Encounter.Alert.11");
    AlertState.Revision = 1;
    AlertState.RemainingSeconds = 4.0f;
    AlertState.bActive = true;
    AlertState.Severity = EGamePlatformUICombatAlertSeverity::Critical;
    TestTrue(TEXT("已授权关键机制警告合法"),
        FGamePlatformUICombatAlertPresentation::IsValidSnapshot(AlertState));
    AlertState.RemainingSeconds = -2.0f;
    TestFalse(TEXT("除未知(-1)以外负数时长非法"),
        FGamePlatformUICombatAlertPresentation::IsValidSnapshot(AlertState));

    return true;
}

/**
 * 验证统一Buff/Debuff（增益/减益）托盘的分区与同类多实例身份，
 * 确保Critical（关键控制）警告单独归类且超额项目不被静默丢弃。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUICombatEffectsTest,
    "GamePlatform.UI.CombatPresentation.StatusEffectGroups",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUICombatEffectsTest::RunTest(const FString&)
{
    FGamePlatformUIStatusEffectTrayState SourceState;
    SourceState.OwnerDisplayId = TEXT("Player.Visible.1");
    SourceState.Revision = 3;

    FGamePlatformUIStatusEffect FirstBuff;
    FirstBuff.EffectId = TEXT("Speed");
    FirstBuff.EffectInstanceId = TEXT("Speed.ByPlayer1");
    FirstBuff.RemainingSeconds = 5.0f;
    SourceState.Effects.Add(FirstBuff);

    FGamePlatformUIStatusEffect SecondBuff = FirstBuff;
    SecondBuff.EffectInstanceId = TEXT("Speed.ByPlayer2");
    SecondBuff.RemainingSeconds = 8.0f;
    SourceState.Effects.Add(SecondBuff);

    FGamePlatformUIStatusEffect Harmful;
    Harmful.EffectId = TEXT("Poison");
    Harmful.bBeneficial = false;
    Harmful.RemainingSeconds = 2.0f;
    SourceState.Effects.Add(Harmful);

    FGamePlatformUIStatusEffect Control;
    Control.EffectId = TEXT("Stunned");
    Control.bBeneficial = false;
    Control.RemainingSeconds = 1.0f;
    Control.Importance = EGamePlatformUIEffectImportance::HardControl;
    SourceState.Effects.Add(Control);

    FGamePlatformUIStatusEffectDisplayPolicy Policy;
    Policy.MaxBeneficial = 1;
    Policy.MaxHarmful = 1;
    Policy.MaxCritical = 1;
    Policy.MaxOther = 0;

    FGamePlatformUIStatusEffectDisplayGroups Groups;
    TestTrue(TEXT("同类型不同施放实例可以共存"),
        FGamePlatformUIStatusEffectPresentation::BuildDisplayGroups(
            SourceState, Policy, Groups));
    TestEqual(TEXT("增益视觉只保留一项"), Groups.Beneficial.Num(), 1);
    TestEqual(TEXT("另一个增益被折叠为+1"), Groups.BeneficialOverflowCount, 1);
    TestEqual(TEXT("减益正常显示"), Groups.Harmful.Num(), 1);
    TestEqual(TEXT("硬控必须进入独立关键通道"), Groups.Critical.Num(), 1);
    TestEqual(TEXT("控制状态不能在普通减益栏重复"), Groups.HarmfulOverflowCount, 0);

    SourceState.Effects[1].EffectInstanceId = TEXT("Speed.ByPlayer1");
    TestFalse(TEXT("重复效果实例身份应拒绝整组快照"),
        FGamePlatformUIStatusEffectPresentation::BuildDisplayGroups(
            SourceState, Policy, Groups));
    return true;
}

#endif
