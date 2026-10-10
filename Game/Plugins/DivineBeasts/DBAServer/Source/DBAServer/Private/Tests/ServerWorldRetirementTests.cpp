// 项目专用服务器世界退休回归：内存世界/生命周期快照仅为测试，不注册服务器、不联网、不生成地图资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
// 原生latent夹具只持有瞬态GI/World/Bootstrap；命令被中止时析构也清理，不创建反射类型或控制面提供者。
struct FQueuedBootstrapRetirementFixture
{
    TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
    TStrongObjectPtr<UWorld> World{UWorld::CreateWorld(EWorldType::Game, false)};
    TStrongObjectPtr<UDivineBeastsServerBootstrapSubsystem> Bootstrap{
        NewObject<UDivineBeastsServerBootstrapSubsystem>(Instance.Get())};
    bool bQueueBarrierRan = false;
    double DeadlineSeconds = FPlatformTime::Seconds() + 10.0;
    ~FQueuedBootstrapRetirementFixture()
    {
        Bootstrap->Deinitialize();
        if (World.Get()) { World->DestroyWorld(false); }
    }
};
class FWaitForBootstrapRetirementQueue final : public IAutomationLatentCommand
{
    TFunction<bool()> UpdateFixture;
public:
    explicit FWaitForBootstrapRetirementQueue(TFunction<bool()> InUpdate) : UpdateFixture(MoveTemp(InUpdate)) {}
    virtual bool Update() override { return UpdateFixture(); }
};
}

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
    Bootstrap->BootstrapOperationId = FGuid::NewGuid();
    Bootstrap->bWorldValidated = true; Bootstrap->State = EDivineBeastsServerBootstrapState::Ready;
    Bootstrap->HandleWorldCleanup(World.Get(), true, true);
    TestFalse(TEXT("旧世界退出撤销世界验证"), Bootstrap->bWorldValidated);
    TestTrue(TEXT("完整世界退出保留退休栅栏"), Bootstrap->bWorldRetired);
    TestFalse(TEXT("世界退出先撤销原Bootstrap操作身份"), Bootstrap->BootstrapOperationId.IsValid());
    TestFalse(TEXT("清理后不再持有已验证世界"), Bootstrap->ValidatedWorld.IsValid());
    FGamePlatformServerLifecycleSnapshot LateReady;
    LateReady.State = EGamePlatformServerLifecycleState::Ready;
    Bootstrap->HandleLifecycleChanged(LateReady);
    TestEqual(TEXT("迟到Ready不能复活旧世界"), Bootstrap->State, EDivineBeastsServerBootstrapState::Failed);
    TestEqual(TEXT("错误明确要求新实例重启"), Bootstrap->LastErrorCode, FName(TEXT("ServerWorldRetiredRestartRequired")));
    Bootstrap->ObserveWorld(World.Get());
    TestFalse(TEXT("退休后不能重新观察未经新Boot授权的世界"), Bootstrap->ObservedWorld.IsValid());
    World->DestroyWorld(false);

    // 真实Registered处理器排队GT任务后立即公开Deinitialize；保持UObject/World存活以检验旧栈而非靠弱指针死亡。
    const auto Queued = MakeShared<FQueuedBootstrapRetirementFixture>();
    if (!TestNotNull(TEXT("原排队任务瞬态世界"), Queued->World.Get())) { return false; }
    Queued->World->SetGameInstance(Queued->Instance.Get());
    Queued->Bootstrap->ObservedWorld = Queued->World.Get(); Queued->Bootstrap->ValidatedWorld = Queued->World.Get();
    Queued->Bootstrap->bWorldValidated = true; Queued->Bootstrap->BootstrapOperationId = FGuid::NewGuid();
    Queued->Bootstrap->ServerBootId = TEXT("test.bootstrap.queue.boot");
    Queued->Bootstrap->State = EDivineBeastsServerBootstrapState::Registering;
    Queued->World->GetTimerManager().SetTimer(Queued->Bootstrap->GameplayBootstrapTimer,
        Queued->Bootstrap.Get(), &UDivineBeastsServerBootstrapSubsystem::AdvanceGameplayBootstrap, 0.1f, true);
    TestTrue(TEXT("夹具拥有真实在途体验Timer"), Queued->World->GetTimerManager().TimerExists(Queued->Bootstrap->GameplayBootstrapTimer));
    FGamePlatformServerLifecycleSnapshot Registered;
    Registered.State = EGamePlatformServerLifecycleState::Registered;
    Queued->Bootstrap->HandleLifecycleChanged(Registered);
    TestTrue(TEXT("真实Registered处理器签发操作身份"), Queued->Bootstrap->BootstrapOperationId.IsValid());
    const FGuid FirstRegisteredOperation = Queued->Bootstrap->BootstrapOperationId;
    Queued->Bootstrap->HandleLifecycleChanged(Registered);
    TestTrue(TEXT("同World/Boot的后继Registered通知也更换原排队操作身份"),
        Queued->Bootstrap->BootstrapOperationId != FirstRegisteredOperation);
    Queued->Bootstrap->Deinitialize();
    const FName ClosedReason = Queued->Bootstrap->LastErrorCode;
    TestTrue(TEXT("公开关闭永久阻止重新承载"), Queued->Bootstrap->bWorldRetired);
    TestFalse(TEXT("公开关闭先失效原排队nonce"), Queued->Bootstrap->BootstrapOperationId.IsValid());
    TestFalse(TEXT("公开关闭撤销自有真实Timer"), Queued->World->GetTimerManager().TimerExists(Queued->Bootstrap->GameplayBootstrapTimer));
    // 同一GT队列的后置屏障证明前面的真实Ready任务已得到执行机会；不直接调用私有lambda或伪造固定成功。
    AsyncTask(ENamedThreads::GameThread, [Queued]() { Queued->bQueueBarrierRan = true; });
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForBootstrapRetirementQueue([this, Queued, ClosedReason]()
    {
        if (!Queued->bQueueBarrierRan)
        {
            if (FPlatformTime::Seconds() <= Queued->DeadlineSeconds) { return false; }
            AddError(TEXT("真实Registered队列屏障未在截止内执行")); return true;
        }
        TestFalse(TEXT("旧排队任务返回后不能重新取得World"), Queued->Bootstrap->ValidatedWorld.IsValid());
        TestFalse(TEXT("旧排队任务不能重新签发关闭nonce"), Queued->Bootstrap->BootstrapOperationId.IsValid());
        TestEqual(TEXT("旧排队任务不能覆盖关闭原因"), Queued->Bootstrap->LastErrorCode, ClosedReason);
        TestTrue(TEXT("关闭后Bootstrap对象仍活着，拒绝来自scope而非GC"), IsValid(Queued->Bootstrap.Get()));
        return true;
    }));

    // 未获得ValidatedWorld也必须永久关闭；这是旧Deinitialize只依赖World存在时遗漏的合法取消边界。
    TStrongObjectPtr<UDivineBeastsServerBootstrapSubsystem> BeforeWorld(NewObject<UDivineBeastsServerBootstrapSubsystem>(Queued->Instance.Get()));
    BeforeWorld->BootstrapOperationId = FGuid::NewGuid(); BeforeWorld->Deinitialize();
    BeforeWorld->ObserveWorld(Queued->World.Get());
    TestTrue(TEXT("未验证世界时公开关闭也保留退休栅栏"), BeforeWorld->bWorldRetired);
    TestFalse(TEXT("关闭后不能观察后续世界"), BeforeWorld->ObservedWorld.IsValid());
    return true;
}
#endif
