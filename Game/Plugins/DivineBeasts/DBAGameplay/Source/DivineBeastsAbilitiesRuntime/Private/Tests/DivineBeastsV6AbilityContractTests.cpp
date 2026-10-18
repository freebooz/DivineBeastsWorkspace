// SSLM V6：在UE5.8原生环境复核技能授权前行为合同；不需要正式96套GA资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Abilities/DivineBeastsV6GameplayAbility.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Types/DivineBeastsAbilityBalanceRow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsV6NativeContractGateTest,
    "DivineBeasts.Abilities.V6.NativeContractFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsV6NativeContractGateTest::RunTest(const FString&)
{
    UDivineBeastsV6GameplayAbility* Ability = NewObject<UDivineBeastsV6GameplayAbility>();
    UDivineBeastsAbilityDefinition* Definition = NewObject<UDivineBeastsAbilityDefinition>();
    if (!TestNotNull(TEXT("V6真实原生技能类"), Ability) ||
        !TestNotNull(TEXT("V6真实项目DataAsset类型"), Definition))
        return false;
    TestEqual(TEXT("V6默认执行方式必须是拒绝模式"),
        Ability->ExecutionMode, EDivineBeastsV6NativeMode::Unsupported);
    TestFalse(TEXT("V6默认定义没有执行资格"), Definition->bV6NativeApproved);
    TestFalse(TEXT("V6原生技能不自动成为草案"), Definition->bEightSlotDraftOnly);
    TestEqual(TEXT("V6只允许ServerOnly权威执行"),
        Ability->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
    TestEqual(TEXT("V6技能必须独立实例持有激活资格"),
        Ability->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);

    FDivineBeastsAbilityBalanceRow Balance;
    Balance.AbilityId = TEXT("dba.ability.rat_eight_fang_cut@1");
    Balance.Level = 1;
    Balance.BaseDamage = 32.f;
    Balance.CooldownSeconds = 0.8f;
    Balance.CastRangeCm = 220.f;

    FString Error;
    TestFalse(TEXT("未批准资源不能误进入V6单体伤害"), Ability->ValidateNativeContract(*Definition, Balance, Error));
    TestFalse(TEXT("未经批准的技能不会静默返回成功"), Error.IsEmpty());
    Ability->ExecutionMode = EDivineBeastsV6NativeMode::ForwardDamage;
    TestFalse(TEXT("仅切换执行模式不能绕过V6审核"), Ability->ValidateNativeContract(*Definition, Balance, Error));

    Definition->bV6NativeApproved = true;
    Definition->bEightSlotDraftOnly = true;
    TestFalse(TEXT("同时标记原生批准及设计草案必须被拒绝"), Ability->ValidateNativeContract(*Definition, Balance, Error));

    Definition->bEightSlotDraftOnly = false;
    TestFalse(TEXT("仍无合法LogicalId、HeroId或已加载数据表时不得授予"), Ability->ValidateNativeContract(*Definition, Balance, Error));
    return true;
}

/** 独立原生数值测试：即使没有96份DataAsset，也能验证受支持的两个控制结果。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsV6ControlPairValidationTest,
    "DivineBeasts.Abilities.V6.DamageControlPairContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsV6ControlPairValidationTest::RunTest(const FString&)
{
    FDivineBeastsEightSlotDraftAction Control;
    Control.ActionKindId = TEXT("control");
    Control.ModifierId = TEXT("stun");
    Control.HitCount = 1;
    Control.Magnitude = 0.0f;
    Control.DurationSeconds = 0.9f;

    TestTrue(TEXT("一次短眩晕合法"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, false));
    TestFalse(TEXT("Stun不能冒充Silence"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));

    Control.ModifierId = TEXT("slow");
    TestFalse(TEXT("平台当前不支持Slow，禁止按眩晕代替"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, false));

    Control.ModifierId = TEXT("silence");
    Control.DurationSeconds = 1.4f;
    TestTrue(TEXT("一次短沉默合法"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));
    TestFalse(TEXT("Silence不能冒充Stun"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, false));

    Control.HitCount = 2;
    TestFalse(TEXT("两段重复控制须专门授权"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));
    Control.HitCount = 1;
    Control.Magnitude = 0.25f;
    TestFalse(TEXT("控制段不能暗带伤害或减速倍率"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));
    Control.Magnitude = 0.0f;
    Control.DurationSeconds = 0.0f;
    TestFalse(TEXT("0秒控制不能消耗技能资源假装生效"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));
    Control.DurationSeconds = 3.1f;
    TestFalse(TEXT("超3秒控制拒绝"),
        UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(Control, true));
    return true;
}
#endif
