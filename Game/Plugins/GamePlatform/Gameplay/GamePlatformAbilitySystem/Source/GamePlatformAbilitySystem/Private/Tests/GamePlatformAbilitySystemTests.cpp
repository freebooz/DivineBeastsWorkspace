#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
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
    TestTrue(TEXT("Ability.Active原生Tag有效"), GamePlatformAbilitySystemTags::Ability_Active.GetTag().IsValid());
    TestTrue(TEXT("Ability.Spell原生Tag有效"), GamePlatformAbilitySystemTags::Ability_Spell.GetTag().IsValid());
    return true;
}

#endif
