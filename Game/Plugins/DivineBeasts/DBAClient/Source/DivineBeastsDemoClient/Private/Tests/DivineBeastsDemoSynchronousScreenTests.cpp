// SSLM演示页面同步回调回归；使用真实原生Screen/VM和世界上下文对象，不打开地图或连接后端。
// 前置：平台允许OpenScreenAsync同步发布Opened；预期同模型成功被接管，其他模型不会被误认。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "UI/DivineBeastsDemoHubScreen.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoSynchronousScreenTest,
    "DivineBeasts.PluginDemos.SynchronousScreenOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoSynchronousScreenTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>());
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> Context(NewObject<UDivineBeastsDemoWorldSubsystem>(World.Get()));
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    TStrongObjectPtr<UDivineBeastsDemoHubScreen> Screen(NewObject<UDivineBeastsDemoHubScreen>());
    Screen->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"),Model.Get(),NAME_None,EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    Context->bOpeningScreen=true;Context->OpeningScreenModel=Model.Get();
    const FGuid Request=FGuid::NewGuid();
    Context->OnDemoScreenOpened(Request,TEXT("DivineBeasts.Demo.Hub"),Screen.Get());
    TestTrue(TEXT("同步打开保存真实活动页面"),Context->ActiveScreen.Get()==Screen.Get());
    TestEqual(TEXT("记录同步完成的请求身份"),Context->SynchronouslyOpenedScreenRequestId,Request);
    Context->ActiveScreen.Reset();Context->OpeningScreenModel.Reset();
    Context->OnDemoScreenOpened(FGuid::NewGuid(),TEXT("DivineBeasts.Demo.Hub"),Screen.Get());
    TestFalse(TEXT("其他模型的同步事件不能接管页面"),Context->ActiveScreen.IsValid());
    return true;
}
#endif
