// 场景效果参数验证：不打开UE地图或替代Niagara GPU测试。
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsBirdFlockFlightSettingsTest,
    "DivineBeasts.BirdFlock.FlightSettings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsBirdFlockFlightSettingsTest::RunTest(const FString& Parameters)
{
    FDivineBeastsBirdFlockFlightSettings Options;
    FString Error;
    TestTrue(TEXT("原盘旋兼容配置有效"),Options.ValidateForWorld(nullptr,Error));
    const auto Copy = Options;
    TestTrue(TEXT("相同参数去重"),Options.EquivalentTo(Copy));
    Options.bUseSpline = true;
    Options.FlightMode = EDivineBeastsBirdFlockFlightMode::SplineOneWay;
    TestFalse(TEXT("无样条源拒绝"),Options.ValidateForWorld(nullptr,Error));
    Options.bUseSpline = false;
    TestFalse(TEXT("模式和开关不一致拒绝"),Options.ValidateForWorld(nullptr,Error));
    Options.FlightMode = EDivineBeastsBirdFlockFlightMode::Orbit;
    Options.bEnableFlocking = true;
    Options.SeparationWeight = 3.f; Options.AlignmentWeight = 2.f;
    TestFalse(TEXT("聚群权重超过总预算拒绝"),Options.ValidateForWorld(nullptr,Error));
    Options.AlignmentWeight = 0.5f;
    TestTrue(TEXT("修复权重后可用"),Options.ValidateForWorld(nullptr,Error));
    Options.QualityFraction = -0.5f;
    TestFalse(TEXT("无效画质参数拒绝"),Options.ValidateForWorld(nullptr,Error));
    return true;
}
#endif
