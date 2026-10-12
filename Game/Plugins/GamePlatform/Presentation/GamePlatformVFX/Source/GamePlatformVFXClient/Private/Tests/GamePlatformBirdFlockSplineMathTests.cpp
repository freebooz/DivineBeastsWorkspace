// 纯路径数学回归：全域边界、重入与非有限值检查，不将本单测当作真实GPU/视觉验收。
#include "Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformBirdSplineMathTest,
    "GamePlatform.VFX.BirdFlock.SplineMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformBirdSplineMathTest::RunTest(const FString& Parameters)
{
    using Mode = EGamePlatformBirdRouteTraversal;
    double Progress = -1.0;
    bool bEnd = false;
    TestTrue(TEXT("一次飞行中段有效"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        5,100,1000,0,Mode::Once,Progress,bEnd));
    TestTrue(TEXT("一次飞行进度0.5"),FMath::IsNearlyEqual(Progress,0.5,0.0001));
    TestFalse(TEXT("中段未到终点"),bEnd);
    TestTrue(TEXT("一次飞行终点限制"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        15,100,1000,0,Mode::Once,Progress,bEnd));
    TestTrue(TEXT("终点到达且不溢出"),bEnd && FMath::IsNearlyEqual(Progress,1.0,0.0001));
    TestTrue(TEXT("循环有效"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        15,100,1000,0,Mode::Loop,Progress,bEnd));
    TestTrue(TEXT("循环进度为0.5"),FMath::IsNearlyEqual(Progress,0.5,0.0001));
    TestTrue(TEXT("往返有效"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        15,100,1000,0,Mode::PingPong,Progress,bEnd));
    TestTrue(TEXT("往返折返点进度0.5"),FMath::IsNearlyEqual(Progress,0.5,0.0001));
    TestFalse(TEXT("零路径长度拒绝"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        1,100,0,0,Mode::Once,Progress,bEnd));
    TestFalse(TEXT("非法个体相位拒绝"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        1,100,1000,1,Mode::Once,Progress,bEnd));
    TestFalse(TEXT("NaN拒绝"),FGamePlatformBirdFlockSplineMath::EvaluateProgress(
        std::numeric_limits<double>::quiet_NaN(),100,1000,0,Mode::Once,Progress,bEnd));
    TestTrue(TEXT("Boids权重有效"),FGamePlatformBirdFlockSplineMath::ValidateFlockingWeights(1,1,1));
    TestFalse(TEXT("Boids权重过量拒绝"),FGamePlatformBirdFlockSplineMath::ValidateFlockingWeights(2,2,2));
    TestEqual(TEXT("合理预算数量"),FGamePlatformBirdFlockSplineMath::ResolveVisibleCount(128,0.5f),64);
    TestEqual(TEXT("非法数量拒绝"),FGamePlatformBirdFlockSplineMath::ResolveVisibleCount(513,1.f),INDEX_NONE);
    return true;
}
#endif
