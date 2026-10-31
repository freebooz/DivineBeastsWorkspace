// 平台交互拥有者完成通知回归：真实World/Actor/动态委托验证快照投递、身份拒绝、作用域撤销和同步重入。
// 只直接驱动生产RPC的接收边界与发送记录，不伪装为实际网络传输；可靠通道/延迟/断线需另做联机验证。
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/InteractionReentrantCommitFixture.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

/** 仅测试友元：不提供生产绕过入口；直接驱动实际请求记录和实际RPC接收函数。 */
struct FInteractionClientCommitTestAccess
{
    static void Remember(UGamePlatformInteractorComponent& Interactor, const FGamePlatformInteractionRequest& Request)
    { Interactor.RememberLocalRequest(Request); }
    static void Receive(UGamePlatformInteractorComponent& Interactor, const FGamePlatformInteractionEvent& Event,
        const FGamePlatformInteractionSession& Session, APlayerController* Controller, APawn* Pawn)
    { Interactor.ClientReceiveCommittedInteraction_Implementation(Event, Session, Controller, Pawn); }
    static void ReplaceCurrentSession(UGamePlatformInteractorComponent& Interactor, const FGamePlatformInteractionSession& Session)
    { Interactor.CurrentSession = Session; }
};

namespace
{
/** 无窗口/无后端的真实引擎夹具；本地玩家和Pawn拥有关系由引擎对象建立，模拟接收代理只改变Actor网络角色。 */
struct FClientCommitFixture
{
    TStrongObjectPtr<ULocalPlayer> Player;
    UWorld* World = nullptr;
    APlayerController* Controller = nullptr;
    APawn* Pawn = nullptr;
    AActor* Target = nullptr;
    AInteractionReentrantCommitFixture* Listener = nullptr;
    UGamePlatformInteractorComponent* Interactor = nullptr;

    bool Initialize(bool bControllerOwner = false)
    {
        if (!GEngine) { return false; }
        const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
        if (!World) { return false; }
        World->InitializeActorsForPlay(FURL());
        Controller = World->SpawnActor<APlayerController>();
        Pawn = World->SpawnActor<APawn>();
        Target = World->SpawnActor<AActor>();
        Listener = World->SpawnActor<AInteractionReentrantCommitFixture>();
        if (!Controller || !Pawn || !Target || !Listener) { return false; }
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        // 仅Transient夹具连接真实Player，避免SetPlayer初始化输入/在线服务等本用例无关外部副作用。
        Controller->Player = Player.Get(); Player->PlayerController = Controller;
        Controller->Possess(Pawn);
        Pawn->SetRole(ROLE_AutonomousProxy);
        AActor* InteractorOwner = bControllerOwner ? static_cast<AActor*>(Controller) : static_cast<AActor*>(Pawn);
        InteractorOwner->SetRole(ROLE_AutonomousProxy);
        Interactor = NewObject<UGamePlatformInteractorComponent>(InteractorOwner);
        InteractorOwner->AddInstanceComponent(Interactor); Interactor->RegisterComponent(); Interactor->BeginPlay();
        Interactor->OnInteractionEvent.AddDynamic(Listener, &AInteractionReentrantCommitFixture::HandleEvent);
        return Controller->IsLocalController() && Pawn->GetController() == Controller;
    }

    ~FClientCommitFixture()
    {
        if (Interactor)
        {
            Interactor->OnInteractionEvent.RemoveAll(Listener);
            Listener->EventHandler = nullptr;
            if (Interactor->HasBegunPlay()) { Interactor->EndPlay(EEndPlayReason::Destroyed); }
        }
        if (Player.IsValid()) { Player->PlayerController = nullptr; }
        if (Controller) { Controller->Player = nullptr; }
        if (World) { World->DestroyWorld(false); }
    }

    FGamePlatformInteractionRequest MakeRequest() const
    {
        FGamePlatformInteractionRequest Request;
        Request.RequestId = FGuid::NewGuid(); Request.TargetActor = Target;
        Request.TargetInstanceId = FGuid::NewGuid(); Request.TargetGeneration = 3;
        Request.ObservedTargetRevision = 8; Request.OptionId = TEXT("Test.OpenPanel");
        return Request;
    }

    FGamePlatformInteractionSession MakeSession(const FGamePlatformInteractionRequest& Request) const
    {
        FGamePlatformInteractionSession Session;
        Session.RequestId = Request.RequestId; Session.SessionId = FGuid::NewGuid(); Session.InteractorActor = Interactor->GetOwner();
        Session.InteractorGeneration = 4; Session.TargetActor = Request.TargetActor;
        Session.TargetInstanceId = Request.TargetInstanceId; Session.TargetGeneration = 3; Session.TargetRevisionAtStart = 8;
        Session.OptionId = TEXT("Test.OpenPanel"); Session.State = EGamePlatformInteractionSessionState::Completed;
        Session.Result.RequestId = Request.RequestId; Session.Result.SessionId = Session.SessionId;
        Session.Result.OptionId = TEXT("Test.OpenPanel"); Session.Result.State = EGamePlatformInteractionSessionState::Completed;
        Session.Result.FinalTargetRevision = 9;
        return Session;
    }

    FGamePlatformInteractionEvent MakeEvent(const FGamePlatformInteractionSession& Session) const
    {
        FGamePlatformInteractionEvent Event;
        Event.EventType = EGamePlatformInteractionEventType::InteractionCommitted;
        Event.RequestId = Session.RequestId; Event.SessionId = Session.SessionId; Event.InteractorActor = Interactor->GetOwner();
        Event.TargetActor = Session.TargetActor; Event.OptionId = TEXT("Test.OpenPanel"); Event.Result = Session.Result;
        return Event;
    }

    void Receive(const FGamePlatformInteractionEvent& Event, const FGamePlatformInteractionSession& Session) const
    { FInteractionClientCommitTestAccess::Receive(*Interactor, Event, Session, Controller, Pawn); }
};
}

// 删除可靠完成桥、依赖RepNotify或把CurrentSession当事件身份，都会丢失本用例中的A成功通知。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionClientCommitSnapshotTest,
    "GamePlatform.Interaction.Network.ClientCommitSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionClientCommitSnapshotTest::RunTest(const FString&)
{
    FClientCommitFixture Fixture;
    if (!TestTrue(TEXT("建立真实本地拥有关系和动态监听"), Fixture.Initialize())) { return false; }
    const auto Request = Fixture.MakeRequest(); const auto Session = Fixture.MakeSession(Request); const auto Event = Fixture.MakeEvent(Session);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, Request);
    auto NewerSession = Session; NewerSession.SessionId = FGuid::NewGuid(); NewerSession.TargetActor = Fixture.Listener;
    FInteractionClientCommitTestAccess::ReplaceCurrentSession(*Fixture.Interactor, NewerSession);
    Fixture.Receive(Event, Session); Fixture.Receive(Event, Session);
    TestEqual(TEXT("重复成功快照只本地投递一次"), Fixture.Listener->Events.Num(), 1);
    if (Fixture.Listener->Events.Num() == 1)
    {
        const auto& Received = Fixture.Listener->Events[0];
        TestEqual(TEXT("沿用A原始目标而非当前B会话"), Received.TargetActor.Get(), Fixture.Target);
        TestEqual(TEXT("完整保留A会话身份"), Received.SessionId, Session.SessionId);
        TestEqual(TEXT("保持服务器确认的修订"), Received.Result.FinalTargetRevision, 9);
    }
    TestEqual(TEXT("通知不改写更新的会话视图"), Fixture.Interactor->GetCurrentSession().SessionId, NewerSession.SessionId);
    // Listen/Standalone的服务器本地Publish已负责通知，RPC接收边界不再回声广播。
    const auto HostRequest = Fixture.MakeRequest(); const auto HostSession = Fixture.MakeSession(HostRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, HostRequest);
    Fixture.Pawn->SetRole(ROLE_Authority); Fixture.Receive(Fixture.MakeEvent(HostSession), HostSession);
    TestEqual(TEXT("权威本地宿主不重复投递"), Fixture.Listener->Events.Num(), 1);
    return true;
}

// 每一项变造均使用已真实记录的请求；缺少任何一处身份/世界/成功校验，都将触发错误的消费通知。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionClientCommitRejectsInvalidIdentityTest,
    "GamePlatform.Interaction.Network.ClientCommitRejectsInvalidIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionClientCommitRejectsInvalidIdentityTest::RunTest(const FString&)
{
    FClientCommitFixture Fixture;
    if (!TestTrue(TEXT("建立接收夹具"), Fixture.Initialize())) { return false; }
    const auto Request = Fixture.MakeRequest(); const auto Session = Fixture.MakeSession(Request); const auto Event = Fixture.MakeEvent(Session);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, Request);
    auto ChangedEvent = Event; ChangedEvent.RequestId = FGuid::NewGuid(); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.SessionId.Invalidate(); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.OptionId = TEXT("Test.Other"); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.InteractorActor = Fixture.Listener; Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.Result.Error = EGamePlatformInteractionError::InvalidTarget; Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.Result.RequestId = FGuid::NewGuid(); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.Result.SessionId = FGuid::NewGuid(); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.Result.OptionId = TEXT("Test.Other"); Fixture.Receive(ChangedEvent, Session);
    ChangedEvent = Event; ChangedEvent.EventType = EGamePlatformInteractionEventType::InteractionStarted; Fixture.Receive(ChangedEvent, Session);
    auto ChangedSession = Session; ChangedSession.RequestId = FGuid::NewGuid(); Fixture.Receive(Event, ChangedSession);
    ChangedSession = Session; ChangedSession.SessionId = FGuid::NewGuid(); Fixture.Receive(Event, ChangedSession);
    ChangedSession = Session; ChangedSession.TargetInstanceId = FGuid::NewGuid(); Fixture.Receive(Event, ChangedSession);
    ChangedSession = Session; ChangedSession.TargetGeneration = 2; Fixture.Receive(Event, ChangedSession);
    ChangedSession = Session; ChangedSession.TargetRevisionAtStart = 7; Fixture.Receive(Event, ChangedSession);
    ChangedSession = Session; ChangedSession.Result.FinalTargetRevision = 10; Fixture.Receive(Event, ChangedSession);
    FInteractionClientCommitTestAccess::Receive(*Fixture.Interactor, Event, Session,
        Fixture.World->SpawnActor<APlayerController>(), Fixture.Pawn);
    FInteractionClientCommitTestAccess::Receive(*Fixture.Interactor, Event, Session,
        Fixture.Controller, Fixture.World->SpawnActor<APawn>());
    TestEqual(TEXT("变造快照均不投递，也不消费合法请求"), Fixture.Listener->Events.Num(), 0);
    Fixture.Receive(Event, Session);
    TestEqual(TEXT("原始快照仍成功投递"), Fixture.Listener->Events.Num(), 1);

    FClientCommitFixture OtherWorld;
    if (!TestTrue(TEXT("建立独立第二世界"), OtherWorld.Initialize())) { return false; }
    auto ForeignRequest = Fixture.MakeRequest(); ForeignRequest.TargetActor = OtherWorld.Target;
    auto ForeignSession = Fixture.MakeSession(ForeignRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, ForeignRequest);
    Fixture.Receive(Fixture.MakeEvent(ForeignSession), ForeignSession);
    const auto DestroyedRequest = Fixture.MakeRequest(); const auto DestroyedSession = Fixture.MakeSession(DestroyedRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, DestroyedRequest);
    Fixture.Target->Destroy(); Fixture.Receive(Fixture.MakeEvent(DestroyedSession), DestroyedSession);
    TestEqual(TEXT("错World和已销毁目标均不投递"), Fixture.Listener->Events.Num(), 1);
    return true;
}

// 旧请求即使Pawn重新被原Controller拥有或页面重新激活也不能复活；清理必须发生于真实生命周期事件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionClientCommitRejectsStaleScopeTest,
    "GamePlatform.Interaction.Network.ClientCommitRejectsStaleScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionClientCommitRejectsStaleScopeTest::RunTest(const FString&)
{
    FClientCommitFixture Fixture;
    if (!TestTrue(TEXT("建立作用域夹具"), Fixture.Initialize())) { return false; }
    const auto OldRequest = Fixture.MakeRequest(); const auto OldSession = Fixture.MakeSession(OldRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, OldRequest);
    Fixture.Pawn->SetRole(ROLE_Authority); Fixture.Controller->UnPossess(); Fixture.Controller->Possess(Fixture.Pawn);
    Fixture.Pawn->SetRole(ROLE_AutonomousProxy); Fixture.Receive(Fixture.MakeEvent(OldSession), OldSession);
    TestEqual(TEXT("拥有关系失去并恢复后拒绝旧请求"), Fixture.Listener->Events.Num(), 0);
    const auto InactiveRequest = Fixture.MakeRequest(); const auto InactiveSession = Fixture.MakeSession(InactiveRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, InactiveRequest);
    Fixture.Interactor->SetLocalFocusPolicy(false, false); Fixture.Interactor->SetLocalFocusPolicy(true, false);
    Fixture.Receive(Fixture.MakeEvent(InactiveSession), InactiveSession);
    TestEqual(TEXT("失活后重显拒绝旧请求"), Fixture.Listener->Events.Num(), 0);
    const auto FreshRequest = Fixture.MakeRequest(); const auto FreshSession = Fixture.MakeSession(FreshRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, FreshRequest); Fixture.Receive(Fixture.MakeEvent(FreshSession), FreshSession);
    TestEqual(TEXT("新作用域的新请求仍可接收"), Fixture.Listener->Events.Num(), 1);
    const auto EndRequest = Fixture.MakeRequest(); const auto EndSession = Fixture.MakeSession(EndRequest);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, EndRequest);
    Fixture.Interactor->EndPlay(EEndPlayReason::LevelTransition); Fixture.Receive(Fixture.MakeEvent(EndSession), EndSession);
    TestEqual(TEXT("EndPlay后拒绝迟到成功"), Fixture.Listener->Events.Num(), 1);

    // Controller宿主身份不变时换Pawn同样要撤销；不能仅比较GetOwner来识别拥有关系代次。
    FClientCommitFixture ControllerFixture;
    if (!TestTrue(TEXT("建立Controller宿主"), ControllerFixture.Initialize(true))) { return false; }
    const auto ControllerRequest = ControllerFixture.MakeRequest(); const auto ControllerSession = ControllerFixture.MakeSession(ControllerRequest);
    FInteractionClientCommitTestAccess::Remember(*ControllerFixture.Interactor, ControllerRequest);
    ControllerFixture.Controller->SetRole(ROLE_Authority);
    ControllerFixture.Controller->UnPossess(); ControllerFixture.Controller->Possess(ControllerFixture.Pawn);
    ControllerFixture.Controller->SetRole(ROLE_AutonomousProxy);
    ControllerFixture.Receive(ControllerFixture.MakeEvent(ControllerSession), ControllerSession);
    TestEqual(TEXT("Controller宿主的旧Pawn代次也被拒绝"), ControllerFixture.Listener->Events.Num(), 0);
    const auto ControllerFreshRequest = ControllerFixture.MakeRequest(); const auto ControllerFreshSession = ControllerFixture.MakeSession(ControllerFreshRequest);
    FInteractionClientCommitTestAccess::Remember(*ControllerFixture.Interactor, ControllerFreshRequest);
    ControllerFixture.Receive(ControllerFixture.MakeEvent(ControllerFreshSession), ControllerFreshSession);
    TestEqual(TEXT("Controller宿主新请求正常接收"), ControllerFixture.Listener->Events.Num(), 1);
    return true;
}

// 广播前消费请求与Session去重；若放到广播之后，监听者内重复消息会再次投递，且可能覆盖重入后的新状态。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionClientCommitReentrantDeliveryTest,
    "GamePlatform.Interaction.Network.ClientCommitReentrantDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionClientCommitReentrantDeliveryTest::RunTest(const FString&)
{
    FClientCommitFixture Fixture;
    if (!TestTrue(TEXT("建立重入夹具"), Fixture.Initialize())) { return false; }
    const auto Request = Fixture.MakeRequest(); const auto Session = Fixture.MakeSession(Request); const auto Event = Fixture.MakeEvent(Session);
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, Request);
    Fixture.Listener->EventHandler = [&]()
    {
        Fixture.Receive(Event, Session);
        Fixture.Interactor->SetLocalFocusPolicy(false, false);
    };
    Fixture.Receive(Event, Session);
    TestEqual(TEXT("真实动态监听重入重复消息也只消费一次"), Fixture.Listener->Events.Num(), 1);
    TestFalse(TEXT("监听者一次回调已完成"), static_cast<bool>(Fixture.Listener->EventHandler));
    Fixture.Interactor->SetLocalFocusPolicy(true, false);
    FGamePlatformInteractionRequest OldestRequest;
    FGamePlatformInteractionRequest NewestRequest;
    for (int32 Index = 0; Index < 70; ++Index)
    {
        NewestRequest = Fixture.MakeRequest();
        if (Index == 0) { OldestRequest = NewestRequest; }
        FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, NewestRequest);
    }
    const auto OldestSession = Fixture.MakeSession(OldestRequest); Fixture.Receive(Fixture.MakeEvent(OldestSession), OldestSession);
    const auto NewestSession = Fixture.MakeSession(NewestRequest); Fixture.Receive(Fixture.MakeEvent(NewestSession), NewestSession);
    TestEqual(TEXT("有界记录拒绝已淘汰旧请求并保留最新请求"), Fixture.Listener->Events.Num(), 2);
    const auto ReusedSessionRequest = Fixture.MakeRequest(); auto ReusedSession = Fixture.MakeSession(ReusedSessionRequest);
    ReusedSession.SessionId = NewestSession.SessionId; ReusedSession.Result.SessionId = NewestSession.SessionId;
    FInteractionClientCommitTestAccess::Remember(*Fixture.Interactor, ReusedSessionRequest);
    Fixture.Receive(Fixture.MakeEvent(ReusedSession), ReusedSession);
    TestEqual(TEXT("同一Session换外层Request也不能重复消费"), Fixture.Listener->Events.Num(), 2);
    return true;
}
#endif
