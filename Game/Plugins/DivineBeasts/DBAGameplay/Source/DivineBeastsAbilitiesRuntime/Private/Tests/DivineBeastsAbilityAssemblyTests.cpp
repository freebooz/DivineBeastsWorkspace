#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"

/**
 * 验证新增业务类的真实反射继承和默认拒绝状态：
 * 无服务器身份/DefaultAbilitySet 的组件不得伪造授权，未配置技能定义的能力不可读数值。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAbilityAssemblyContractTest,
    "DivineBeasts.Abilities.Assembly.Contract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityAssemblyContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("正式项目角色继承ACharacter"),
        ADivineBeastsGameplayCharacter::StaticClass()->IsChildOf(ACharacter::StaticClass()));
    TestTrue(TEXT("项目技能类继承平台唯一GAS能力基类"),
        UDivineBeastsConfiguredGameplayAbility::StaticClass()->IsChildOf(
            UGamePlatformGameplayAbility::StaticClass()));

    // Actor/抽象 Ability 在UE测试中只读 CDO，禁止通过 NewObject 直接创建真实 Actor 或抽象类。
    const ADivineBeastsGameplayCharacter* Character =
        GetDefault<ADivineBeastsGameplayCharacter>();
    TestNotNull(TEXT("项目角色反射对象可创建"), Character);
    if (!Character)
    {
        return false;
    }
    TestNotNull(TEXT("角色携带唯一GAS能力组件"),
        Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>());
    TestNotNull(TEXT("角色携带已有身份组件"),
        Character->FindComponentByClass<UDivineBeastsCharacterComponent>());
    TestNotNull(TEXT("角色携带平台战斗组件"),
        Character->FindComponentByClass<UGamePlatformCombatComponent>());

    UDivineBeastsAbilityLoadoutComponent* Loadout =
        Character->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>();
    TestNotNull(TEXT("角色携带技能授予状态组件"), Loadout);
    if (!Loadout)
    {
        return false;
    }
    TestFalse(TEXT("初始无真实授权时不得显示为已就绪"),
        Loadout->GetLoadoutState().bReady);
    TestEqual(TEXT("初始没有可释放或虚构的技能"), Loadout->GetLoadoutState().Slots.Num(), 0);

    FDivineBeastsAbilityBalanceRow Row;
    FString Error;
    const UDivineBeastsConfiguredGameplayAbility* Ability =
        GetDefault<UDivineBeastsConfiguredGameplayAbility>();
    TestNotNull(TEXT("项目技能基类默认对象存在"), Ability);
    if (Ability)
    {
        TestFalse(TEXT("没有已加载主资产时拒绝读取伤害参数"),
            Ability->TryReadConfiguredBalance(Row, Error));
    }
    return true;
}
#endif
