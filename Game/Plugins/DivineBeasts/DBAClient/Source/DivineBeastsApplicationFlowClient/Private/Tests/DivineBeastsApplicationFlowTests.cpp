#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GamePlatformApplicationFlowSubsystem.h"
#include "GamePlatformApplicationFlowTypes.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "DivineBeastsApplicationFlowSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowNodeRegistrationTest,
    "DivineBeasts.ApplicationFlow.NodeRegistrationAndGeneration",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowNodeRegistrationTest::RunTest(const FString&)
{
    UGamePlatformApplicationFlowSubsystem* Flow =
        NewObject<UGamePlatformApplicationFlowSubsystem>();
    TestNotNull(TEXT("Platform Flow must construct"), Flow);
    if (!Flow)
    {
        return false;
    }

    FGamePlatformFlowNodeDefinition Boot;
    Boot.NodeId = FDivineBeastsFlowNodes::Boot();
    Boot.AllowedNextNodes = { FDivineBeastsFlowNodes::Initialize() };

    FGamePlatformFlowNodeDefinition Initialize;
    Initialize.NodeId = FDivineBeastsFlowNodes::Initialize();
    Initialize.AllowedNextNodes = { FDivineBeastsFlowNodes::Authentication() };

    TestTrue(TEXT("Boot register"), Flow->RegisterNode(Boot));
    TestFalse(TEXT("Duplicate node rejected"), Flow->RegisterNode(Boot));
    TestTrue(TEXT("Initialize register"), Flow->RegisterNode(Initialize));

    FGuid RunId;
    TestTrue(TEXT("StartRun"), Flow->StartRun(Boot.NodeId, RunId));
    TestTrue(TEXT("FlowRunId valid"), RunId.IsValid());

    const FGamePlatformFlowOperationToken BeforeTransition = Flow->BeginOperation();
    TestTrue(TEXT("Operation token valid"), BeforeTransition.IsValid());
    TestTrue(TEXT("Current operation"), Flow->IsOperationCurrent(BeforeTransition));

    TestTrue(TEXT("Transition"), Flow->TransitionTo(Initialize.NodeId));
    const FGamePlatformFlowSnapshot Snapshot = Flow->GetSnapshot();
    TestTrue(TEXT("Generation advanced"), Snapshot.NodeGeneration > BeforeTransition.NodeGeneration);
    TestFalse(TEXT("Old callback stale after transition"), Flow->IsOperationCurrent(BeforeTransition));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowTypeBoundaryTest,
    "DivineBeasts.ApplicationFlow.TypeBoundary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowTypeBoundaryTest::RunTest(const FString&)
{
    FDivineBeastsCharacterCreateDraft Draft;
    TestFalse(TEXT("Empty draft invalid"), Draft.IsLocallyValid());

    Draft.HeroDefinitionId = TEXT("Hero.Rat");
    Draft.CharacterName = TEXT("RatHero");
    TestTrue(TEXT("Minimal current draft valid"), Draft.IsLocallyValid());

    TestEqual(
        TEXT("Persistent selection action is explicit"),
        StaticEnum<EDivineBeastsFlowAction>()->GetNameStringByValue(
            static_cast<int64>(EDivineBeastsFlowAction::SelectPersistentCharacter)),
        FString(TEXT("SelectPersistentCharacter")));

    TestTrue(
        TEXT("Login error model contains invalid credentials"),
        StaticEnum<EDivineBeastsFlowError>()->IsValidEnumValue(
            static_cast<int64>(EDivineBeastsFlowError::InvalidCredentials)));
    TestTrue(
        TEXT("Login error model contains timeout"),
        StaticEnum<EDivineBeastsFlowError>()->IsValidEnumValue(
            static_cast<int64>(EDivineBeastsFlowError::TimedOut)));

    return true;
}

namespace
{
    class FTestFlowExtension final : public IDivineBeastsApplicationFlowExtension
    {
    public:
        virtual void OnEnteredInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            ++Entered;
        }

        virtual void OnLeavingInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            ++Left;
        }

        int32 Entered = 0;
        int32 Left = 0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowExtensionRegistryTest,
    "DivineBeasts.ApplicationFlow.ExtensionRegistry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowExtensionRegistryTest::RunTest(const FString&)
{
    UDivineBeastsApplicationFlowSubsystem* Flow =
        NewObject<UDivineBeastsApplicationFlowSubsystem>();
    TestNotNull(TEXT("Project Flow must construct"), Flow);
    if (!Flow)
    {
        return false;
    }

    const TSharedRef<FTestFlowExtension> Extension =
        MakeShared<FTestFlowExtension>();
    const FName ExtensionId(TEXT("Test.Arena.Extension"));

    TestTrue(TEXT("Extension register"), Flow->RegisterExtension(ExtensionId, Extension));
    TestFalse(TEXT("Duplicate extension rejected"), Flow->RegisterExtension(ExtensionId, Extension));
    TestTrue(TEXT("Extension unregister"), Flow->UnregisterExtension(ExtensionId));
    TestFalse(TEXT("Stale unregister safe"), Flow->UnregisterExtension(ExtensionId));
    return true;
}

#endif
