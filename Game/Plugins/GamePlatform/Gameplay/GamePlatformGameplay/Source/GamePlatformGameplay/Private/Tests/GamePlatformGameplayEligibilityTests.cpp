#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformGameplayEligibilitySnapshotTest,
    "GamePlatform.Gameplay.EligibilitySnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformGameplayEligibilitySnapshotTest::RunTest(const FString& Parameters)
{
    UGamePlatformGameplayEligibilityComponent* Component =
        NewObject<UGamePlatformGameplayEligibilityComponent>();

    const FGamePlatformGameplayEligibilitySnapshot Snapshot =
        Component->GetSnapshot();
    TestFalse(TEXT("默认未进入Gameplay Active"), Snapshot.bActive);
    TestEqual(TEXT("默认AvatarGeneration为1"), Snapshot.AvatarGeneration, 1);

    Component->SetServerPlayerActive(true);
    TestFalse(
        TEXT("没有Authority Owner时不能伪造Active"),
        Component->IsServerPlayerActiveForGameplay());
    return true;
}

#endif
