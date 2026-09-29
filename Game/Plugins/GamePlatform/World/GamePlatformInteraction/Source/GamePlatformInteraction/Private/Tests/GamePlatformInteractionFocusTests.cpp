#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Services/GamePlatformInteractionFocusRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInteractionFocusOrderTest,
    "GamePlatform.Interaction.Focus.DeterministicSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformInteractionFocusOrderTest::RunTest(const FString& Parameters)
{
    FGamePlatformInteractionOption Low;
    Low.OptionId = TEXT("Low");
    Low.Priority = 1;
    Low.MaxDistance = 300.0f;

    FGamePlatformInteractionOption HighB;
    HighB.OptionId = TEXT("High.B");
    HighB.Priority = 10;
    HighB.MaxDistance = 250.0f;

    FGamePlatformInteractionOption HighA = HighB;
    HighA.OptionId = TEXT("High.A");

    TArray<FGamePlatformInteractionOption> Options;
    Options.Add(Low);
    Options.Add(HighB);
    Options.Add(HighA);

    FGamePlatformInteractionOption Selected;
    TestTrue(
        TEXT("距离内应有候选"),
        FGamePlatformInteractionFocusRules::SelectBestOption(
            Options,
            100.0f,
            Selected));

    TestEqual(
        TEXT("同优先级稳定按OptionId选择"),
        Selected.OptionId,
        FName(TEXT("High.A")));

    TestFalse(
        TEXT("全部超距时无候选"),
        FGamePlatformInteractionFocusRules::SelectBestOption(
            Options,
            500.0f,
            Selected));

    FGamePlatformInteractionOption Wider = HighA;
    Wider.OptionId = TEXT("High.Near");
    Wider.MaxDistance = 300.0f;

    FGamePlatformInteractionOption Tighter = Wider;
    Tighter.OptionId = TEXT("High.Far");
    Tighter.MaxDistance = 200.0f;

    TestTrue(
        TEXT("同优先级时更短MaxDistance应优先"),
        FGamePlatformInteractionFocusRules::IsPreferredOption(
            Tighter,
            &Wider));
    TestFalse(
        TEXT("更长MaxDistance不能覆盖更严格候选"),
        FGamePlatformInteractionFocusRules::IsPreferredOption(
            Wider,
            &Tighter));

    return true;
}

#endif
