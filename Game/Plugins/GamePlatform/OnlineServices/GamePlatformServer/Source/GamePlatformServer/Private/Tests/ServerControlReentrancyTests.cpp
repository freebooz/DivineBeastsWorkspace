// 平台控制回归：显式内存状态模拟Register瞬时失败，不调用Provider/网络，验证通知内排空/关闭的真实重试所有权。
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformServerControlReentrancyTest, "GamePlatform.Server.Control.ReentrantRetryOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformServerControlReentrancyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformServerLifecycleSubsystem> Service(NewObject<UGamePlatformServerLifecycleSubsystem>(Instance.Get()));
    Service->Generation = 1; Service->bControlOperationInFlight = true;
    Service->Snapshot.State = EGamePlatformServerLifecycleState::Registering;
    Service->OnLifecycleChanged().AddLambda([&](const auto&)
    {
        TestTrue(TEXT("失败通知内排空只受理为排队"), Service->BeginDrain());
    });
    Service->CompleteOperation(1, UGamePlatformServerLifecycleSubsystem::EControlOperation::Register, false, TEXT("ControlPlaneTransportFailed"));
    TestTrue(TEXT("排空不能越过Register重试"), Service->PendingControlRetryOperation.IsSet());
    TestTrue(TEXT("排空意图保留到Register完成"), Service->DeferredControlOperation.IsSet());
    TestEqual(TEXT("原Register仍持有过渡态"), Service->Snapshot.State, EGamePlatformServerLifecycleState::Registering);
    Service->Deinitialize();
    TStrongObjectPtr<UGamePlatformServerLifecycleSubsystem> Closing(NewObject<UGamePlatformServerLifecycleSubsystem>(Instance.Get()));
    Closing->Generation = 1; Closing->bControlOperationInFlight = true;
    Closing->Snapshot.State = EGamePlatformServerLifecycleState::Registering;
    Closing->OnLifecycleChanged().AddLambda([&](const auto&) { Closing->Deinitialize(); });
    Closing->CompleteOperation(1, UGamePlatformServerLifecycleSubsystem::EControlOperation::Register, false, TEXT("ControlPlaneTransportFailed"));
    TestTrue(TEXT("通知内关闭取消重试Ticker"), !Closing->ControlRetryTickerHandle.IsValid());
    TestFalse(TEXT("通知返回不得重新安排重试"), Closing->PendingControlRetryOperation.IsSet());
    TestFalse(TEXT("关闭后不允许新的Drain"), Closing->BeginDrain());
    return true;
}
#endif
