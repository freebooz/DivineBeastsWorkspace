// 默认村庄双鸟群的实际入口回归：自有世界与本地玩家；只暂停内容发布以观察意图，不伪造资源成功。
// 测试涵盖稳定出生点选择、朝向、分屏、重复调用及世界清理；Niagara/GPU另由真实PIE核验。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageDefaultBirdsTest,
    "DivineBeasts.Presentation.VillageBirds.DefaultLayoutAndWorldCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageDefaultBirdsTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("需要真实引擎"), GEngine)) return false;
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/DBAWorldPack_Village/Tests/Birds_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, Package, false));
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World.Get());
    const TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    World->SetGameInstance(Instance.Get());
    Context.OwningGameInstance = Instance.Get();
    const TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    Context.GameViewport = Viewport.Get();
    Viewport->Init(Context, Instance.Get(), false);
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    Instance->AddLocalPlayer(Player.Get(), FPlatformUserId::CreateFromInternalId(0));
    Player->ViewportClient = Viewport.Get();
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Service(NewObject<UDivineBeastsPresentationClientSubsystem>(Player.Get()));
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    Service->PlatformPresentation = Platform.Get();
    Service->ScopeId = FGuid::NewGuid();
    Service->EnvironmentFlockContextSuspensions = 1; // 真实“上下文切换暂停”路径，不请求资产或假装Pack已Active。
    FActorSpawnParameters Later; Later.Name = TEXT("PlayerStart_1");
    World->SpawnActor<APlayerStart>(FVector(240, 0, 120), FRotator::ZeroRotator, Later);
    FActorSpawnParameters First; First.Name = TEXT("PlayerStart_0");
    World->SpawnActor<APlayerStart>(FVector(0, 0, 120), FRotator(0, 90, 0), First);
    Service->StartDefaultWorldBirdFlock(World.Get());
    TestEqual(TEXT("海鸥32只意图"), Service->GetRequestedBirdCount(TEXT("DBA.Environment.WorldBirds")), 32);
    TestEqual(TEXT("燕子24只意图"), Service->GetRequestedBirdCount(TEXT("DBA.Environment.VillageSwallows")), 24);
    const auto* Seagulls = Service->BirdFlocks.Requests.Find(TEXT("DBA.Environment.WorldBirds"));
    const auto* Swallows = Service->BirdFlocks.Requests.Find(TEXT("DBA.Environment.VillageSwallows"));
    if (TestNotNull(TEXT("海鸥实际意图"), Seagulls) && TestNotNull(TEXT("燕子实际意图"), Swallows))
    {
        TestTrue(TEXT("命名出生点与朝向决定海鸥位置"), Seagulls->Origin.Equals(FVector(0, 600, 470), .01));
        TestTrue(TEXT("燕子不同低空中心"), Swallows->Origin.Equals(FVector(650, 1400, 370), .01));
        TestEqual(TEXT("合法物种沿用已有定义"), Swallows->SpeciesId, FName(TEXT("Swallow")));
    }
    Service->StartDefaultWorldBirdFlock(World.Get());
    TestEqual(TEXT("重复入口不重复建立新群"), Service->BirdFlocks.Requests.Num(), 2);
    const TStrongObjectPtr<ULocalPlayer> Second(NewObject<ULocalPlayer>(GEngine));
    Instance->AddLocalPlayer(Second.Get(), FPlatformUserId::CreateFromInternalId(1));
    Second->ViewportClient = Viewport.Get();
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> SecondService(NewObject<UDivineBeastsPresentationClientSubsystem>(Second.Get()));
    SecondService->StartDefaultWorldBirdFlock(World.Get());
    TestEqual(TEXT("分屏第二玩家不建立默认群"), SecondService->BirdFlocks.Requests.Num(), 0);
    Service->HandleWorldCleanup(World.Get(), true, true);
    TestEqual(TEXT("世界清理两群均撤销"), Service->BirdFlocks.Requests.Num(), 0);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World.Get());
    return true;
}
#endif
