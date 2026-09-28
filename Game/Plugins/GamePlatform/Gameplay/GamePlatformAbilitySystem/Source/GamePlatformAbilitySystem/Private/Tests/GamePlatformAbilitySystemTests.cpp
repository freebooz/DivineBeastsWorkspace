#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "Tags/GamePlatformAbilitySystemTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAbilitySystemBaseTest,
    "GamePlatform.AbilitySystem.BaseContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAbilitySystemBaseTest::RunTest(const FString& Parameters)
{
    UGamePlatformAbilitySystemComponent* ASC =
        NewObject<UGamePlatformAbilitySystemComponent>();

    const FGamePlatformAbilityAvatarBindingSnapshot Snapshot =
        ASC->GetAvatarBindingSnapshot();
    TestFalse(TEXT("默认未绑定Avatar"), Snapshot.bBound);
    TestEqual(TEXT("默认AvatarGeneration为0"), Snapshot.AvatarGeneration, 0);
    TestFalse(TEXT("空Owner/Avatar不能绑定"), ASC->BindAbilityActorInfo(nullptr, nullptr));
    TestFalse(TEXT("未绑定Avatar时输入令牌必须无效"), ASC->GetInputToken().IsValid());

    const UGamePlatformGameplayAbility* AbilityCDO =
        Cast<UGamePlatformGameplayAbility>(UGamePlatformGameplayAbility::StaticClass()->GetDefaultObject());
    TestNotNull(TEXT("平台GameplayAbility基类必须存在CDO"), AbilityCDO);
    TestTrue(TEXT("平台GameplayAbility默认使用InstancedPerActor"),
        AbilityCDO && AbilityCDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor);
    TestTrue(TEXT("平台GameplayAbility默认输入策略为OnPressed"),
        AbilityCDO && AbilityCDO->ActivationPolicy == EGamePlatformAbilityActivationPolicy::OnPressed);
    TestTrue(TEXT("平台AttributeSet基类必须继承原生UAttributeSet"),
        UGamePlatformAttributeSet::StaticClass()->IsChildOf(UAttributeSet::StaticClass()));
    TestTrue(TEXT("Ability.Active原生Tag有效"), GamePlatformAbilitySystemTags::Ability_Active.GetTag().IsValid());
    TestTrue(TEXT("Ability.Spell原生Tag有效"), GamePlatformAbilitySystemTags::Ability_Spell.GetTag().IsValid());
    return true;
}

#endif
