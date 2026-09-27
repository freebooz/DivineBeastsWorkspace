#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "GamePlatformPresentationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXPresentationProviderNullWorldTest,
    "GamePlatform.VFX.Presentation.ProviderNullWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXPresentationProviderNullWorldTest::RunTest(const FString& Parameters)
{
    const FGamePlatformVFXPresentationProvider Provider(nullptr);
    FGamePlatformPresentationRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.SemanticTag = FGameplayTag::RequestGameplayTag(
        TEXT("Presentation.Test.VFX"),
        false);
    Request.ProviderChannel = TEXT("VFX");
    Request.DefinitionId = TEXT("VFX.Test.NullWorld");

    TestFalse(
        TEXT("无 World/Service 时Provider必须确定性拒绝"),
        Provider.Handle(Request));
    return true;
}

#endif
