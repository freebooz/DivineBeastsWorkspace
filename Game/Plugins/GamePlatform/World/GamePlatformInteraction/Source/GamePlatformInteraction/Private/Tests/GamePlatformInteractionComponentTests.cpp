#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Types/GamePlatformInteractionRequest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInteractionComponentConstructionTest,
    "GamePlatform.Interaction.Components.Construction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformInteractionComponentConstructionTest::RunTest(const FString& Parameters)
{
    UGamePlatformInteractorComponent* Interactor =
        NewObject<UGamePlatformInteractorComponent>();
    UGamePlatformInteractableComponent* Interactable =
        NewObject<UGamePlatformInteractableComponent>();

    TestNotNull(TEXT("InteractorComponent可创建"), Interactor);
    TestNotNull(TEXT("InteractableComponent可创建"), Interactable);

    FGamePlatformInteractionRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.TargetInstanceId = FGuid::NewGuid();
    Request.TargetGeneration = 2;
    Request.ObservedTargetRevision = 7;
    Request.OptionId = TEXT("Test.Option");

    TestTrue(TEXT("RequestId有效"), Request.RequestId.IsValid());
    TestTrue(TEXT("TargetInstanceId有效"), Request.TargetInstanceId.IsValid());
    TestEqual(TEXT("Generation保存"), Request.TargetGeneration, 2);
    TestEqual(TEXT("Revision保存"), Request.ObservedTargetRevision, 7);

    return true;
}

#endif
