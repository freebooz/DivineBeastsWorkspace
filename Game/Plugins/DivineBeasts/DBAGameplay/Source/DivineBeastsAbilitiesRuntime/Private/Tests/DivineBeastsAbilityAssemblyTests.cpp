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
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"

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

/**
 * 与原始角色装配测试解耦的独立门禁；只有新二进制模块加载成功后，
 * 编辑器自动化列表才会出现这条测试，避免误将旧DLL的测试成功视为新逻辑通过。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsReversibleStartupEffectTest,
    "DivineBeasts.Abilities.GrantTransaction.ReversibleStartupEffect",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsReversibleStartupEffectTest::RunTest(const FString&)
{
    FString Error;
    // 测试服务器启动Effect的事务可撤销门禁，无需英雄数据或独立世界。
    UGameplayEffect* StartupEffect = NewObject<UGameplayEffect>();
    StartupEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
    StartupEffect->Period.SetValue(0.0f);
    TestTrue(TEXT("非堆叠、非周期无限启动效果允许"),
        UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
            *StartupEffect, 1.0f, Error));
    StartupEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
    TestFalse(TEXT("瞬时效果不能作为可回滚技能集合启动授予"),
        UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
            *StartupEffect, 1.0f, Error));
    StartupEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
    StartupEffect->Period.SetValue(2.0f);
    TestFalse(TEXT("周期效果拒绝用于默认技能集合，避免回滚前造成不可逆伤害"),
        UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
            *StartupEffect, 1.0f, Error));
    StartupEffect->Period.SetValue(0.0f);
    StartupEffect->Executions.AddDefaulted();
    TestFalse(TEXT("自定义执行计算不能作为可回滚默认授予"),
        UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
            *StartupEffect, 1.0f, Error));
    StartupEffect->Executions.Reset();

#if WITH_EDITOR
    // GameplayEffect::SetStackingType为Editor私有非导出接口；测试通过UE标准反射
    // 设置UPROPERTY，不跨模块调用不导出的链接符号，避免Editor以外Target依赖。
    const FEnumProperty* StackProperty = FindFProperty<FEnumProperty>(
        UGameplayEffect::StaticClass(), FName(TEXT("StackingType")));
    TestNotNull(TEXT("引擎仍提供堆叠类型反射字段"), StackProperty);
    if (StackProperty)
    {
        void* ValueAddress = StackProperty->ContainerPtrToValuePtr<void>(StartupEffect);
        StackProperty->GetUnderlyingProperty()->SetIntPropertyValue(
            ValueAddress, static_cast<int64>(EGameplayEffectStackingType::AggregateBySource));
        TestFalse(TEXT("堆叠启动效果可能覆盖其他来源的句柄，必须拒绝"),
            UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
                *StartupEffect, 1.0f, Error));
    }
#endif

    return true;
}
#endif
