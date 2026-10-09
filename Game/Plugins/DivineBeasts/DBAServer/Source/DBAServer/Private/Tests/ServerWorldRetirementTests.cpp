// 项目专用服务器世界退休回归：内存世界/生命周期快照仅为测试，不注册服务器、不联网、不生成地图资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsServerWorldRetirementTest, "DivineBeasts.Server.WorldRetirement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsServerWorldRetirementTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    World->SetGameInstance(Instance.Get());
    TStrongObjectPtr<UDivineBeastsServerBootstrapSubsystem> Bootstrap(NewObject<UDivineBeastsServerBootstrapSubsystem>(Instance.Get()));
    // 设置明确的“已验证旧世界”测试前提，直接验真实清理/生命周期回调，不替代Profile/网络注册验收。
    Bootstrap->ObservedWorld = World.Get(); Bootstrap->ValidatedWorld = World.Get();
    Bootstrap->bWorldValidated = true; Bootstrap->State = EDivineBeastsServerBootstrapState::Ready;
    Bootstrap->HandleWorldCleanup(World.Get(), true, true);
    TestFalse(TEXT("旧世界退出撤销世界验证"), Bootstrap->bWorldValidated);
    TestTrue(TEXT("完整世界退出保留退休栅栏"), Bootstrap->bWorldRetired);
    TestFalse(TEXT("清理后不再持有已验证世界"), Bootstrap->ValidatedWorld.IsValid());
    FGamePlatformServerLifecycleSnapshot LateReady;
    LateReady.State = EGamePlatformServerLifecycleState::Ready;
    Bootstrap->HandleLifecycleChanged(LateReady);
    TestEqual(TEXT("迟到Ready不能复活旧世界"), Bootstrap->State, EDivineBeastsServerBootstrapState::Failed);
    TestEqual(TEXT("错误明确要求新实例重启"), Bootstrap->LastErrorCode, FName(TEXT("ServerWorldRetiredRestartRequired")));
    Bootstrap->ObserveWorld(World.Get());
    TestFalse(TEXT("退休后不能重新观察未经新Boot授权的世界"), Bootstrap->ObservedWorld.IsValid());
    World->DestroyWorld(false);
    return true;
}
#endif
