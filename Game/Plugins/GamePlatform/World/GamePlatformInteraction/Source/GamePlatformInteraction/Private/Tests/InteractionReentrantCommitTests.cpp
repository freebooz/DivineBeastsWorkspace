// 平台交互同步重入回归：Custom选项替换撤销原提交；终态监听者启动B时，A事实保身份且不覆盖B视图。
// 测试只驱动本模块的提交/终态边界，无地图内容、后端或网络副作用；夹具拥有World、委托和自有会话计时器。
#include "Tests/InteractionReentrantCommitFixture.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Types/GamePlatformInteractionSession.h"
bool AInteractionReentrantCommitFixture::CommitInteraction(const FGamePlatformInteractionSession&, const FGamePlatformInteractionOption&)
{ return FindComponentByClass<UGamePlatformInteractableComponent>()->SetOptions({}); }
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionReentrantCommitTest,
    "GamePlatform.Interaction.Lifecycle.CustomCommitReentrantOptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionReentrantCommitTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Owner = World->SpawnActor<AInteractionReentrantCommitFixture>();
    auto* Target = NewObject<UGamePlatformInteractableComponent>(Owner); Owner->AddInstanceComponent(Target); Target->RegisterComponent();
    auto* Interactor = NewObject<UGamePlatformInteractorComponent>(Owner); Owner->AddInstanceComponent(Interactor); Interactor->RegisterComponent();
    Target->BeginPlay();
    FGamePlatformInteractionOption Option; Option.OptionId = TEXT("Test.Custom"); Option.CommitKind = EGamePlatformInteractionCommitKind::Custom;
    TestTrue(TEXT("配置Custom选项"), Target->SetOptions({Option}));
    FGamePlatformInteractionSession Session; Session.SessionId = FGuid::NewGuid(); Session.RequestId = FGuid::NewGuid();
    Session.TargetGeneration = Target->GetTargetGeneration(); Session.TargetInstanceId = Target->GetTargetInstanceId();
    EGamePlatformInteractionError Error; TestTrue(TEXT("会话真实取得目标占位"), Target->TryAcquireSession(Session.SessionId, Interactor, Option, Error));
    const int32 Revision = Target->GetTargetRevision();
    FGamePlatformInteractionResult Result;
    TestFalse(TEXT("回调取消后不能写入成功终态"), Target->CommitSession(Session, *Target->FindOption(Option.OptionId), Result));
    TestTrue(TEXT("回调后选项确实失效"), Target->FindOption(Option.OptionId) == nullptr);
    TestEqual(TEXT("只保留配置修改的修订号，不额外发布提交"), Target->GetTargetRevision(), Revision + 1);
    World->DestroyWorld(false); return true;
}
// OnResultChanged同步开启另一目标B后，A的成功事件仍必须保留A目标/选项；A会话更新不得覆盖B。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionTerminalReentryTest,
    "GamePlatform.Interaction.Lifecycle.TerminalListenerStartsDifferentTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionTerminalReentryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    if (!TestNotNull(TEXT("终态重入真实World"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    // CreateWorld只初始化UWorld，不初始化Actor运行阶段。锁定UE5.8 AActor::ProcessEvent要求
    // AreActorsInitialized才路由普通UFUNCTION；否则Result/Event/Session三个动态监听全部被静默挡住。
    // 使用正式入口建立该前提，不改全局脚本许可或给监听函数加CallInEditor绕过门禁。
    World->InitializeActorsForPlay(FURL());
    if (!TestTrue(TEXT("Actor已通过正式初始化，允许真实动态委托路由"), World->AreActorsInitialized())) { return false; }
    auto* Listener = World->SpawnActor<AInteractionReentrantCommitFixture>(); auto* TargetActorA = World->SpawnActor<AActor>();
    auto* TargetActorB = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("真实终态监听Actor"), Listener) || !TestNotNull(TEXT("真实目标A"), TargetActorA) ||
        !TestNotNull(TEXT("真实目标B"), TargetActorB)) { return false; }
    auto* Interactor = NewObject<UGamePlatformInteractorComponent>(Listener);
    Listener->AddInstanceComponent(Interactor); Interactor->RegisterComponent();
    const auto MakeTarget = [](AActor* Owner)
    { auto* Target = NewObject<UGamePlatformInteractableComponent>(Owner); Owner->AddInstanceComponent(Target);
        Target->RegisterComponent(); Target->BeginPlay(); return Target; };
    auto* TargetA = MakeTarget(TargetActorA); auto* TargetB = MakeTarget(TargetActorB);
    FGamePlatformInteractionOption OptionA; OptionA.OptionId = TEXT("Option.A"); OptionA.Mode = EGamePlatformInteractionMode::Hold;
    FGamePlatformInteractionOption OptionB = OptionA; OptionB.OptionId = TEXT("Option.B");
    TargetA->SetOptions({OptionA}); TargetB->SetOptions({OptionB});
    const auto MakeRequest = [](UGamePlatformInteractableComponent* Target, FName OptionId)
    { FGamePlatformInteractionRequest Request; Request.RequestId = FGuid::NewGuid(); Request.TargetActor = Target->GetOwner();
        Request.TargetInstanceId = Target->GetTargetInstanceId(); Request.TargetGeneration = Target->GetTargetGeneration();
        Request.ObservedTargetRevision = Target->GetTargetRevision(); Request.OptionId = OptionId; return Request; };
    const auto RequestA = MakeRequest(TargetA, OptionA.OptionId); const auto RequestB = MakeRequest(TargetB, OptionB.OptionId);
    Interactor->StartSession(RequestA, *TargetA, OptionA, 1); const auto SessionA = Interactor->GetCurrentSession();
    TestEqual(TEXT("A真实取得目标会话"), SessionA.State, EGamePlatformInteractionSessionState::Active);
    Interactor->OnResultChanged.AddDynamic(Listener, &AInteractionReentrantCommitFixture::HandleResult);
    Interactor->OnInteractionEvent.AddDynamic(Listener, &AInteractionReentrantCommitFixture::HandleEvent);
    Interactor->OnSessionChanged.AddDynamic(Listener, &AInteractionReentrantCommitFixture::HandleSession);
    bool bStartedBInsideTerminalListener = false;
    // 先摘测试委托/栈引用，再取消本夹具自有会话与Hold计时器，最后由外层guard销毁World；失败退出也清理。
    ON_SCOPE_EXIT
    {
        Interactor->OnResultChanged.RemoveAll(Listener); Interactor->OnInteractionEvent.RemoveAll(Listener);
        Interactor->OnSessionChanged.RemoveAll(Listener); Listener->ResultHandler = nullptr;
        Interactor->CancelSession(EGamePlatformInteractionCancelReason::UserCancelled, EGamePlatformInteractionError::UserCancelled);
    };
    Listener->ResultHandler = [&]()
    {
        Interactor->StartSession(RequestB, *TargetB, OptionB, 1);
        bStartedBInsideTerminalListener = Interactor->GetCurrentSession().RequestId == RequestB.RequestId &&
            Interactor->GetCurrentSession().State == EGamePlatformInteractionSessionState::Active;
    };
    FGamePlatformInteractionResult ResultA; ResultA.RequestId = RequestA.RequestId; ResultA.SessionId = SessionA.SessionId;
    ResultA.State = EGamePlatformInteractionSessionState::Completed;
    Interactor->FinishSession(ResultA, EGamePlatformInteractionEventType::InteractionCommitted);
    TestTrue(TEXT("真实终态动态监听已消费一次回调"), !Listener->ResultHandler);
    TestTrue(TEXT("终态监听内部确实启动B"), bStartedBInsideTerminalListener);
    TestEqual(TEXT("B真实取得独立目标占位"), TargetB->GetOccupancyCount(), 1);
    const auto* PublishedA = Listener->Events.FindByPredicate([&](const auto& Event)
        { return Event.EventType == EGamePlatformInteractionEventType::InteractionCommitted && Event.SessionId == SessionA.SessionId; });
    TestNotNull(TEXT("A终态事实仍发布"), PublishedA);
    if (PublishedA) { TestEqual(TEXT("A事件保留原目标"), PublishedA->TargetActor.Get(), TargetActorA);
        TestEqual(TEXT("A事件保留原选项"), PublishedA->OptionId, OptionA.OptionId); }
    TestEqual(TEXT("当前会话仍为B"), Interactor->GetCurrentSession().RequestId, RequestB.RequestId);
    TestFalse(TEXT("不能在B开始后广播A会话覆盖B视图"), Listener->Sessions.ContainsByPredicate([&](const auto& Session)
        { return Session.SessionId == SessionA.SessionId; }));
    return true;
}
#endif
