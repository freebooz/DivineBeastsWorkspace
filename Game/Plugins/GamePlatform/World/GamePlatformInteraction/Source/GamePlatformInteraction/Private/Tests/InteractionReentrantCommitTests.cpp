// 引擎组件重入回归：Custom回调中的选项替换必须使本次提交失败，不写成功缓存/不推进取消后的修订号。
#include "Tests/InteractionReentrantCommitFixture.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Types/GamePlatformInteractionSession.h"
bool AInteractionReentrantCommitFixture::CommitInteraction(const FGamePlatformInteractionSession&, const FGamePlatformInteractionOption&)
{ return FindComponentByClass<UGamePlatformInteractableComponent>()->SetOptions({}); }
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
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
    auto* Listener = World->SpawnActor<AInteractionReentrantCommitFixture>(); auto* TargetActorA = World->SpawnActor<AActor>();
    auto* TargetActorB = World->SpawnActor<AActor>(); auto* Interactor = NewObject<UGamePlatformInteractorComponent>(Listener);
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
    Listener->ResultHandler = [&]() { Interactor->StartSession(RequestB, *TargetB, OptionB, 1); };
    FGamePlatformInteractionResult ResultA; ResultA.RequestId = RequestA.RequestId; ResultA.SessionId = SessionA.SessionId;
    ResultA.State = EGamePlatformInteractionSessionState::Completed;
    Interactor->FinishSession(ResultA, EGamePlatformInteractionEventType::InteractionCommitted);
    const auto* PublishedA = Listener->Events.FindByPredicate([&](const auto& Event) { return Event.SessionId == SessionA.SessionId; });
    TestNotNull(TEXT("A终态事实仍发布"), PublishedA);
    if (PublishedA) { TestEqual(TEXT("A事件保留原目标"), PublishedA->TargetActor.Get(), TargetActorA);
        TestEqual(TEXT("A事件保留原选项"), PublishedA->OptionId, OptionA.OptionId); }
    TestEqual(TEXT("当前会话仍为B"), Interactor->GetCurrentSession().RequestId, RequestB.RequestId);
    TestFalse(TEXT("不能在B开始后广播A会话覆盖B视图"), Listener->Sessions.ContainsByPredicate([&](const auto& Session)
        { return Session.SessionId == SessionA.SessionId; }));
    Interactor->OnResultChanged.RemoveAll(Listener); Interactor->OnInteractionEvent.RemoveAll(Listener);
    Interactor->OnSessionChanged.RemoveAll(Listener); World->DestroyWorld(false); return true;
}
#endif
