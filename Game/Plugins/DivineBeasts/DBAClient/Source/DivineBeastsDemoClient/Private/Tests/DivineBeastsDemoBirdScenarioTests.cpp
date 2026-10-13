// SSLM鸟群适配拒绝/清理回归；只构造瞬态开发对象，不生成Niagara或把请求数量冒充GPU实数。
// 正常场景的真实显示与恢复仍需T09资产及运行证据，不能由这里的空场景否定路径替代。
#if WITH_DEV_AUTOMATION_TESTS
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoBirdScenarioTest, "DivineBeasts.PluginDemos.BirdScenario",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoBirdScenarioTest::RunTest(const FString&)
{
    auto Adapter = DivineBeasts::Demo::CreateBirdFlockScenario();
    FDivineBeastsDemoScenarioContext Context;
    Context.Token.RunId = FGuid::NewGuid(); Context.Token.WorldId = FGuid::NewGuid(); Context.Token.Generation = 1;
    int32 Completions = 0; FGamePlatformResult Result;
    const auto Capture = [&](const auto& Token, const auto& Actual) { TestTrue(TEXT("携回完整身份"), Token == Context.Token); ++Completions; Result = Actual; };
    Adapter->Prepare(Context, Capture);
    TestEqual(TEXT("无正式世界准备只完成一次"), Completions, 1);
    TestEqual(TEXT("无真实鸟群拒绝而非固定成功"), Result.Code, FName(TEXT("Demo.BirdSceneUnavailable")));
    FDivineBeastsDemoParameterValue Invalid; Invalid.Type = EDivineBeastsDemoParameterType::Integer; Invalid.IntegerValue = -1;
    Adapter->Execute(TEXT("Execute"), {Invalid}, Capture);
    TestFalse(TEXT("无准备/非法数量不能修改真实组件"), Result.IsSuccess());
    Adapter->Stop(Capture); TestTrue(TEXT("未接管资源的停止无其他组件副作用"), Result.IsSuccess());
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>());
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> Controller(NewObject<UDivineBeastsDemoWorldSubsystem>(World.Get()));
    TestEqual(TEXT("未实读GPU显示未知"), Controller->GetActualBirdCount(), -1);
    TestFalse(TEXT("瞬态空控制器没有原值所有权"), Controller->HasPendingLegacyEdits());
    TestTrue(TEXT("无记录的局部恢复不影响任何Batch"), Controller->RestoreBirdEdits());
    return true;
}
#endif
