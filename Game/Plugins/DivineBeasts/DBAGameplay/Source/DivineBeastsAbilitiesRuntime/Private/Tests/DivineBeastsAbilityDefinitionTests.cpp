#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Types/DivineBeastsAbilityBalanceRow.h"
#include <limits>

/** 测试纯数据路径：校验正反样例；不依赖真实未交付的技能二进制资产。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAbilityDataValidationTest,
    "DivineBeasts.Abilities.DefinitionValidation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityDataValidationTest::RunTest(const FString&)
{
    FDivineBeastsAbilityBalanceRow Row;
    FString Error;
    TestFalse(TEXT("未设置稳定技能ID的数值行拒绝"), Row.Validate(Error));

    Row.AbilityId = TEXT("dba.ability.rat_example@1");
    Row.Level = 1;
    Row.BaseDamage = 60.0f;
    Row.CooldownSeconds = 3.0f;
    TestTrue(TEXT("合法技能数值行接受"), Row.Validate(Error));

    Row.AttackPowerCoefficient = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("非有限伤害系数拒绝"), Row.Validate(Error));
    Row.AttackPowerCoefficient = 0.5f;
    Row.MomentumCost = -1.0f;
    TestFalse(TEXT("负气势消耗拒绝"), Row.Validate(Error));
    Row.MomentumCost = 10.0f;
    TestTrue(TEXT("恢复合法成本"), Row.Validate(Error));

    UDivineBeastsAbilityDefinition* Definition = NewObject<UDivineBeastsAbilityDefinition>();
    TestFalse(TEXT("空定义拒绝"), Definition->ValidateDefinition().IsSuccess());
    TestFalse(TEXT("未加载表不得同步读取数值"), Definition->TryGetLoadedBalance(1, Row, Error));
    return true;
}
#endif
