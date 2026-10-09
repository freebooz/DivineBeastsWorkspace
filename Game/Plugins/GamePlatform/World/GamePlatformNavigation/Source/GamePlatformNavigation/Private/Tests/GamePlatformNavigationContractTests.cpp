#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformNavigationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformNavigationContractTest,
    "GamePlatform.Navigation.Contracts.PathResultAndHandle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformNavigationContractTest::RunTest(const FString& Parameters)
{
    FGamePlatformNavigationRequestHandle Handle;
    TestFalse(TEXT("默认Handle无效"), Handle.IsValid());

    Handle.RequestId = FGuid::NewGuid();
    Handle.WorldGeneration = 3;
    TestFalse(TEXT("旧两字段句柄缺操作代次必须失效"), Handle.IsValid());
    Handle.OperationGeneration = 1;
    TestTrue(TEXT("RequestId+WorldGeneration+OperationGeneration形成有效Handle"), Handle.IsValid());

    FGamePlatformNavigationPathResult Result;
    Result.PathLength = 1200.0f;
    Result.PathCost = 50.0f;
    TestEqual(TEXT("PathLength保持UE厘米语义"), Result.PathLength, 1200.0f);
    TestEqual(TEXT("PathCost独立于距离"), Result.PathCost, 50.0f);

    return true;
}
#endif
