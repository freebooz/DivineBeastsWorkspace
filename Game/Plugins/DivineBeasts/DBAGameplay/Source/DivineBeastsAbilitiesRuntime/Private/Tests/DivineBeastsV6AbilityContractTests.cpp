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
#endif
