#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformInteractionOption.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInteractionOptionValidationTest,
    "GamePlatform.Interaction.Option.StructuralValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformInteractionOptionValidationTest::RunTest(const FString& Parameters)
{
    FGamePlatformInteractionOption Instant;
    Instant.OptionId = TEXT("Door.Toggle");
    Instant.Mode = EGamePlatformInteractionMode::Instant;
    Instant.MaxDistance = 250.0f;
    TestTrue(TEXT("合法Instant选项"), Instant.IsStructurallyValid());

    FGamePlatformInteractionOption Hold = Instant;
    Hold.OptionId = TEXT("Harvest.Gather");
    Hold.Mode = EGamePlatformInteractionMode::Hold;
    Hold.HoldDuration = 2.0f;
    TestTrue(TEXT("合法Hold选项"), Hold.IsStructurallyValid());

    Hold.HoldDuration = 0.0f;
    TestFalse(TEXT("Hold时长必须大于0"), Hold.IsStructurallyValid());

    Instant.OptionId = NAME_None;
    TestFalse(TEXT("OptionId不能为空"), Instant.IsStructurallyValid());

    return true;
}

#endif
