#include "Misc/AutomationTest.h"

#include "Performance/GamePlatformPerformanceTestRunner.h"
#include "Review/GamePlatformReviewTypes.h"
#include "Validation/GamePlatformValidationService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsRulesTest,
    "GamePlatform.DeveloperTools.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsRulesTest::RunTest(const FString& Parameters)
{
    const TArray<FGamePlatformValidationRule>& Rules = FGamePlatformValidationService::GetRules();
    TestTrue(TEXT("Built-in rules are registered"), Rules.Num() >= 10);
    TestNotNull(TEXT("Dependency rule exists"), FGamePlatformValidationService::FindRule(TEXT("GP.Dependency")));
    TestTrue(
        TEXT("Removed Element tag is rejected"),
        FGamePlatformValidationService::IsRemovedLegacyGameplayTag(TEXT("Element.Fire")));
    TestFalse(
        TEXT("Current neutral tag is not rejected"),
        FGamePlatformValidationService::IsRemovedLegacyGameplayTag(TEXT("Ability.Active")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsAllowlistTest,
    "GamePlatform.DeveloperTools.Allowlist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsAllowlistTest::RunTest(const FString& Parameters)
{
    FGamePlatformValidationAllowlistEntry Entry;
    Entry.RuleId = TEXT("GP.Naming");
    Entry.Target = TEXT("/Game/Test");
    Entry.Reason = TEXT("Temporary migration");
    Entry.Owner = TEXT("Owner");
    Entry.ApprovedBy = TEXT("Reviewer");
    Entry.CreatedAt = FDateTime::UtcNow();
    Entry.ExpiresAt = Entry.CreatedAt + FTimespan::FromDays(1);
    Entry.Ticket = TEXT("DEV-1");

    TArray<FGamePlatformValidationAllowlistEntry> Entries{Entry};

    TestTrue(
        TEXT("Active allowlist applies"),
        FGamePlatformValidationService::IsAllowlisted(
            Entry.RuleId,
            Entry.Target,
            Entries,
            Entry.CreatedAt));

    TestFalse(
        TEXT("Expired allowlist no longer applies"),
        FGamePlatformValidationService::IsAllowlisted(
            Entry.RuleId,
            Entry.Target,
            Entries,
            Entry.ExpiresAt + FTimespan::FromSeconds(1)));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsReportPathTest,
    "GamePlatform.DeveloperTools.ReportPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsReportPathTest::RunTest(const FString& Parameters)
{
    FString Directory;
    FString Error;
    TestFalse(
        TEXT("Path traversal RunId is rejected"),
        FGamePlatformValidationService::ResolveSafeReportDirectory(
            TEXT("../escape"),
            Directory,
            Error));

    TestTrue(
        TEXT("Secret masking never returns original full value"),
        FGamePlatformValidationService::MaskSensitiveValue(TEXT("abcdef"))
            != TEXT("abcdef"));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsReviewTest,
    "GamePlatform.DeveloperTools.ManualReview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsReviewTest::RunTest(const FString& Parameters)
{
    UGamePlatformReviewCase* ReviewCase = NewObject<UGamePlatformReviewCase>();
    ReviewCase->CaseId = TEXT("Review.Case");
    ReviewCase->Category = TEXT("Smoke");
    ReviewCase->Description = TEXT("Manual smoke review");
    ReviewCase->Steps = { TEXT("Observe expected UI") };
    ReviewCase->ExpectedResult = TEXT("Expected UI is correct");
    ReviewCase->RequiredEvidence = { TEXT("Screenshot") };

    FString Error;
    TestTrue(TEXT("Review case is complete"), ReviewCase->IsCaseComplete(Error));

    UGamePlatformReviewSubsystem* Subsystem = NewObject<UGamePlatformReviewSubsystem>();
    UGamePlatformReviewReport* Report =
        Subsystem->CreatePendingReport(ReviewCase, TEXT("Run-1"), TEXT("Build-1"), TEXT("Content-1"));

    TestNotNull(TEXT("Pending report created"), Report);
    TestEqual(
        TEXT("AI-created report remains NotRun"),
        Report->Status,
        EGamePlatformReviewStatus::NotRun);

    TArray<FString> Evidence{TEXT("evidence.png")};
    TestFalse(
        TEXT("Without explicit human confirmation report cannot be marked Passed"),
        Subsystem->ApplyHumanDecision(
            Report,
            EGamePlatformReviewStatus::Passed,
            TEXT("Reviewer"),
            TEXT(""),
            Evidence,
            false,
            Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsPerformanceTest,
    "GamePlatform.DeveloperTools.PerformanceBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsPerformanceTest::RunTest(const FString& Parameters)
{
    UGamePlatformPerformanceTestRunner* Runner = NewObject<UGamePlatformPerformanceTestRunner>();

    FGamePlatformPerformanceBaseline A;
    A.ScenarioId = TEXT("Scenario.MainArena");
    A.HardwareProfile = TEXT("HW-A");
    A.EngineVersion = TEXT("5.8");
    A.Samples = 100;

    FGamePlatformPerformanceBaseline B = A;

    FString Reason;
    TestTrue(TEXT("Compatible baseline can compare"), Runner->CanCompareBaselines(A, B, Reason));

    B.HardwareProfile = TEXT("HW-B");
    TestFalse(TEXT("Different hardware cannot compare"), Runner->CanCompareBaselines(A, B, Reason));

    return true;
}

#endif
