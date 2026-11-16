#include "Tests/SessionTravelFixture.h"
void ASessionTravelFixture::ClientTravelInternal_Implementation(const FString&, ETravelType, bool, FGuid)
{
    ++TravelCalls;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Transport/GamePlatformUESessionTransport.h"
#include "Components/GamePlatformAdmissionHandshakeComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

// 捕获Village->Arena时Ticker尚见旧世界：不能把旧握手组件当作目标连接，烧掉新准入证明。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionDepartureWorldTest,
    "GamePlatform.Session.Transport.DepartureWorldCannotSubmitAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSessionDepartureWorldTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<ASessionTravelFixture>();
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    Controller->SetPlayer(Player.Get());
    auto* Component = NewObject<UGamePlatformAdmissionHandshakeComponent>(Controller);
    Controller->AddInstanceComponent(Component); Component->RegisterComponent();
    if (!TestTrue(TEXT("旧世界已有真实本地控制器和握手组件"),
        Controller->IsLocalController() && World->GetFirstPlayerController() == Controller)) return false;

    auto Transport = CreateGamePlatformUESessionTransport(*Instance);
    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = FGuid::NewGuid(); Request.Endpoint = TEXT("127.0.0.1:17881");
    Request.TicketId = TEXT("offline-fixture-ticket");
    Request.TransferTicket = TEXT("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    auto& Binding = Request.ExpectedBinding;
    Binding.AssignmentId = TEXT("destination-assignment"); Binding.GameSessionId = TEXT("destination-session");
    Binding.ServerInstanceId = TEXT("destination-server"); Binding.ServerBootId = TEXT("destination-boot");
    Binding.WorldId = TEXT("World.Test.Destination"); Binding.ProtocolVersion = TEXT("1"); Binding.SessionEpoch = 1;
    int32 NetworkFacts = 0;
    FGamePlatformSessionTransportCallbacks Callbacks;
    Callbacks.OnFact = [&](EGamePlatformSessionTransferFact Fact, const FGamePlatformSessionConnectionBinding&)
    {
        if (Fact == EGamePlatformSessionTransferFact::NetworkConnected) ++NetworkFacts;
        // RED也在发送任何证明之前撤销，保证测试不调用准入后端。
        Transport->CancelTransfer(Request.TransferOperationId);
    };
    Transport->BeginTransfer(Request, MoveTemp(Callbacks));
    ON_SCOPE_EXIT { Transport->CancelTransfer(Request.TransferOperationId); Transport.Reset(); };
    TestEqual(TEXT("实际Transport已发起旅行边界，但替身没有更换World"), Controller->TravelCalls, 1);
    FTSTicker::GetCoreTicker().Tick(0.1f);
    TestEqual(TEXT("旧World的握手组件不能发布目标已连接事实"), NetworkFacts, 0);
    return true;
}
#endif
