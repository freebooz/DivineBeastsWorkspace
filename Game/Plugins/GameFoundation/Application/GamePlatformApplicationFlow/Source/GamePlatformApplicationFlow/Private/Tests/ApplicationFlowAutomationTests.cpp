#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/ApplicationFlowExecutorCases.h"

/** UE 测试直接运行生产执行器，不替代为另一个状态机。每个场景独立列出。 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGamePlatformFlowAutomation,
    "GamePlatform.ApplicationFlow.Core",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

void FGamePlatformFlowAutomation::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const auto& Case : GamePlatform::ApplicationFlow::Tests::GetCases())
    {
        Names.Add(UTF8_TO_TCHAR(Case.Name));
        Commands.Add(UTF8_TO_TCHAR(Case.Name));
    }
}

bool FGamePlatformFlowAutomation::RunTest(const FString& Parameters)
{
    for (const auto& Case : GamePlatform::ApplicationFlow::Tests::GetCases())
    {
        if (Parameters != UTF8_TO_TCHAR(Case.Name)) continue;
        GamePlatform::ApplicationFlow::Tests::FChecks Checks;
        Case.Run(Checks);
        for (const auto& Failure : Checks.Failures) AddError(UTF8_TO_TCHAR(Failure.c_str()));
        return Checks.Failures.empty();
    }
    AddError(TEXT("未知流程测试场景"));
    return false;
}
#endif
