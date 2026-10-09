// 平台服务器准入回归：用显式测试账本验证退出、通知重入及同OperationId旧代次，绝不签票、联网或代表生产授权。
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformServerAdmissionResetTest, "GamePlatform.Server.Admission.ResetTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformServerAdmissionResetTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues));
    World->SetGameInstance(Instance.Get());
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("测试世界产生真实Controller对象"), Controller)) { World->DestroyWorld(false); return false; }
    TStrongObjectPtr<UGamePlatformServerAdmissionSubsystem> Service(NewObject<UGamePlatformServerAdmissionSubsystem>(Instance.Get()));
    FGamePlatformServerAdmissionTarget Target;
    Target.GameServerId = TEXT("test-server"); Target.ServerBootId = TEXT("test-boot");
    Target.WorldId = TEXT("test-world"); Target.ExperienceId = TEXT("test-experience");
    Target.ProtocolVersion = TEXT("2"); Target.ServerStartGeneration = 1;
    TestTrue(TEXT("完整目标配置受理"), Service->ConfigureTarget(Target));
    const FGuid OperationId = FGuid::NewGuid();
    int32 CancelledCompletions = 0;
    UGamePlatformServerAdmissionSubsystem::FPendingAdmission Pending;
    Pending.OperationId = OperationId; Pending.Generation = 7; Pending.Controller = Controller;
    Pending.Completion = [&CancelledCompletions](auto Result) { if (!Result.bSucceeded) ++CancelledCompletions; };
    Service->PendingAdmissions.Add(OperationId, MoveTemp(Pending));
    FGamePlatformServerVerifiedAdmission Admission;
    Admission.AdmissionId = FGuid::NewGuid();
    Service->VerifiedAdmissions.Add(Controller, Admission);
    Service->StopAcceptingAdmissions();
    TestTrue(TEXT("正常排空保留已有连接账本"), Service->VerifiedAdmissions.Contains(Controller));
    TestTrue(TEXT("正常排空保留目标用于已有连接查询"), Service->ActiveTarget.IsValid());
    TestEqual(TEXT("正常排空取消尚未发布的握手"), CancelledCompletions, 1);
    TestFalse(TEXT("排空目标不能直接重配重新打开新准入"), Service->ConfigureTarget(Target));
    bool bRevoked = false;
    Service->OnAdmissionChanged().AddLambda([&](const APlayerController*, const auto&, bool bValid)
    {
        bRevoked = !bValid;
        TestFalse(TEXT("撤销通知重入不得重配目标"), Service->ConfigureTarget(Target));
    });
    Service->ResetTarget();
    TestTrue(TEXT("退出必须广播连接撤销"), bRevoked);
    TestEqual(TEXT("在途请求完成一次失败"), CancelledCompletions, 1);
    TestTrue(TEXT("本地已验证账本清空"), Service->VerifiedAdmissions.IsEmpty());
    TestTrue(TEXT("退出后可显式配置新目标"), Service->ConfigureTarget(Target));
    UGamePlatformServerAdmissionSubsystem::FPendingAdmission Retry;
    Retry.OperationId = OperationId; Retry.Generation = 8; Retry.Controller = Controller;
    Service->PendingAdmissions.Add(OperationId, MoveTemp(Retry));
    Service->CompleteAdmission(OperationId, 7, {});
    TestTrue(TEXT("同ID旧代次回调不得移除新请求"), Service->PendingAdmissions.Contains(OperationId));
    Service->OnAdmissionChanged().Clear();
    Service->ResetTarget();
    // 正常排空的取消通知可以同时触发世界退出；停止门闩不能屏蔽更强的完整撤销。
    TestTrue(TEXT("为停止重入退出夹具配置目标"), Service->ConfigureTarget(Target));
    Service->VerifiedAdmissions.Add(Controller, Admission);
    UGamePlatformServerAdmissionSubsystem::FPendingAdmission ExitDuringStop;
    ExitDuringStop.OperationId = FGuid::NewGuid(); ExitDuringStop.Controller = Controller;
    ExitDuringStop.Completion = [&](auto) { Service->ResetTarget(TEXT("TestWorldRetired")); };
    const FGuid ExitOperation = ExitDuringStop.OperationId;
    Service->PendingAdmissions.Add(ExitOperation, MoveTemp(ExitDuringStop));
    Service->StopAcceptingAdmissions();
    TestTrue(TEXT("停止回调内世界退出必须清空已验证账本"), Service->VerifiedAdmissions.IsEmpty());
    TestFalse(TEXT("停止回调内世界退出必须撤销目标"), Service->ActiveTarget.IsValid());
    TestTrue(TEXT("为停止重入反初始化夹具配置目标"), Service->ConfigureTarget(Target));
    Service->VerifiedAdmissions.Add(Controller, Admission);
    UGamePlatformServerAdmissionSubsystem::FPendingAdmission CloseDuringStop;
    CloseDuringStop.OperationId = FGuid::NewGuid(); CloseDuringStop.Controller = Controller;
    CloseDuringStop.Completion = [&](auto) { Service->Deinitialize(); };
    const FGuid CloseOperation = CloseDuringStop.OperationId;
    Service->PendingAdmissions.Add(CloseOperation, MoveTemp(CloseDuringStop));
    Service->StopAcceptingAdmissions();
    TestTrue(TEXT("停止回调内反初始化必须清空已验证账本"), Service->VerifiedAdmissions.IsEmpty());
    TestFalse(TEXT("反初始化后永久拒绝重开目标"), Service->ConfigureTarget(Target));
    World->DestroyWorld(false);
    return true;
}
#endif
