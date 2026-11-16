// 项目适配生命周期回归：真实Actor销毁与平台同步通知，不加载Niagara、不冒充GPU或网络验收。
// Active账本及受理provider仅为同步重入夹具；生产内容包仍必须通过真实Data预载事务。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicLifecycleTest,
    "DivineBeasts.Presentation.DevelopmentMagic.CleanupAndSynchronousReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicLifecycleTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
    if(!TestNotNull(TEXT("隔离真实World"),World.Get())) return false;
    World->InitializeActorsForPlay(FURL());
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Service(NewObject<UDivineBeastsPresentationClientSubsystem>(Player.Get()));
    TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    Service->DevelopmentMagicWorld=World.Get(); Service->ScopeId=FGuid::NewGuid(); Service->PlatformPresentation=Platform.Get();
    // 未开始、未加载资源的弹体同样必须解绑，不允许弱键在长期Village世界无限积累。
    auto* Pending=World->SpawnActor<AGamePlatformCombatProjectile>(); Pending->DispatchBeginPlay();
    Service->HandleDevelopmentMagicSpawn(Pending);
    TestEqual(TEXT("加载前记录一个订阅"),Service->DevelopmentMagicFlights.Num(),1);
    Pending->Destroy();
    TestEqual(TEXT("Pack未Active且事实未开始时销毁也立即清理"),Service->DevelopmentMagicFlights.Num(),0);
    Service->DevelopmentMagicFlights.Reset();
    FDivineBeastsPresentationContentPackHandle Pack; Pack.Id=FGuid::NewGuid(); Pack.ScopeId=Service->ScopeId; Pack.Generation=1;
    UDivineBeastsPresentationClientSubsystem::FActivePack Active; Active.Handle=Pack;
    Service->DevelopmentMagicPack=Pack; Service->ActivePacks.Add(Pack.Id,Active);
    auto* Projectile=World->SpawnActor<AGamePlatformCombatProjectile>(); Projectile->DispatchBeginPlay();
    Service->HandleDevelopmentMagicSpawn(Projectile);
    FGamePlatformCombatProjectileFacts Facts; Facts.bStarted=true; Facts.OccurrenceId=FGuid::NewGuid();
    Facts.PresentationStyleId=TEXT("DBA.Development.Fireball"); Facts.SourceAvatarGeneration=1;
    int32 FlightSubmissions=0; bool bReentered=false;
    const FGuid Provider=Platform->RegisterProvider(TEXT("Test.Magic.Reentry"),0,
        FGamePlatformPresentationProviderHandler::CreateLambda([](const FGamePlatformPresentationRequest&){return true;}));
    const FDelegateHandle Observer=Platform->OnRequestObserved().AddLambda(
        [&](const FGamePlatformPresentationRequest& Request,EGamePlatformPresentationSubmitResult)
        {
            if(Request.PredictionState==EGamePlatformPresentationPredictionState::Cancelled) return;
            ++FlightSubmissions;
            if(!bReentered) { bReentered=true; Service->HandleDevelopmentMagicFacts(Projectile,Facts); }
        });
    Service->HandleDevelopmentMagicFacts(Projectile,Facts);
    TestEqual(TEXT("同步观察者重入同事实只播放一次"),FlightSubmissions,1);
    Service->HandleDevelopmentMagicFacts(Projectile,Facts);
    TestEqual(TEXT("普通重复事实不重播"),FlightSubmissions,1);
    Platform->OnRequestObserved().Remove(Observer);
    Platform->UnregisterProvider(Provider);
    // 即使VFX提供者拒绝命中请求，已预载的命中SFX仍须独立受理且只播放一次。
    Service->StarterAudioWorld = World.Get(); Service->StarterSpellPack = Pack;
    const auto FireImpact = FGameplayTag::RequestGameplayTag(TEXT("Presentation.SSLM.Audio.Spell.Fire.Impact"));
    int32 ImpactAudioCount = 0;
    const FGuid AudioProvider = Platform->RegisterProvider(TEXT("Test.Magic.AudioOnly"), 0,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        { return Request.SemanticTag == FireImpact; }));
    const auto AudioObserver = Platform->OnRequestObserved().AddLambda(
        [&](const FGamePlatformPresentationRequest& Request, EGamePlatformPresentationSubmitResult Result)
        {
            if (Request.SemanticTag == FireImpact && Result == EGamePlatformPresentationSubmitResult::Submitted)
            {
                ++ImpactAudioCount;
                TestTrue(TEXT("音效使用服务器复制命中中心"), Request.SourceLocation.Equals(Facts.ImpactLocation));
                Service->HandleDevelopmentMagicFacts(Projectile, Facts);
            }
        });
    Facts.bFinished = true; Facts.bImpacted = true; Facts.ImpactLocation = FVector(200, 20, 100);
    Service->HandleDevelopmentMagicFacts(Projectile, Facts);
    Service->HandleDevelopmentMagicFacts(Projectile, Facts);
    TestEqual(TEXT("命中VFX拒绝及同步重入均不吞掉或重复命中音效"), ImpactAudioCount, 1);
    Platform->OnRequestObserved().Remove(AudioObserver);
    Platform->UnregisterProvider(AudioProvider);
    Service->StarterSpellPack = {}; Service->StarterAudioWorld.Reset();
    Service->UnbindDevelopmentMagicWorld();
    TestEqual(TEXT("解绑后所有自有条目撤销"),Service->DevelopmentMagicFlights.Num(),0);
    World->DestroyWorld(false);
    return true;
}
/** 真实GI/LocalPlayer/Viewport/Data事务，不伪造Active账本，防止条目数量漂移把整包关闭。 */
struct FStarterSpellActivationState
{
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UGameViewportClient> Viewport;
    // Editor每帧会清理无底层视口的LocalPlayer；持有真实SceneViewport覆盖整个异步事务。
    TSharedPtr<FSceneViewport> SceneViewport;
    TWeakObjectPtr<ULocalPlayer> Player;
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> Service;
    UWorld* World=nullptr;
    FDivineBeastsPresentationContentPackHandle Pack;
    double Started=FPlatformTime::Seconds();
    ~FStarterSpellActivationState()
    {
        if(!Instance.IsValid()) return;
        if(Service.IsValid() && Pack.IsValid()) Service->DeactivateContentPack(Pack);
        if(Player.IsValid() && Instance->GetLocalPlayers().Contains(Player.Get())) Instance->RemoveLocalPlayer(Player.Get());
        if(World) World->EndPlay(EEndPlayReason::Quit);
        Instance->Shutdown();
        if(World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    }
};
class FWaitStarterSpellActivation final : public IAutomationLatentCommand
{
public:
    FWaitStarterSpellActivation(FAutomationTestBase& InTest,TSharedPtr<FStarterSpellActivationState> InState):Test(InTest),State(InState) {}
    bool Update() override
    {
        if(!Test.TestTrue(TEXT("异步事务期间本地玩家仍归真实GI持有"),State->Instance->GetLocalPlayers().Contains(State->Player.Get()))) return true;
        State->World->Tick(LEVELTICK_All,.05f);
        FString Error;
        const auto Status=State->Service->GetContentPackState(State->Pack,Error);
        if(Status==EDivineBeastsPresentationContentPackState::Loading && FPlatformTime::Seconds()-State->Started<30) return false;
        Test.TestEqual(TEXT("真实法术包Data预载和目录发布完成：")+Error,Status,EDivineBeastsPresentationContentPackState::Active);
        return true;
    }
private:
    FAutomationTestBase& Test; TSharedPtr<FStarterSpellActivationState> State;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsStarterSpellActivationTest,
    "DivineBeasts.Presentation.StarterAudio.RealSpellPackActivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsStarterSpellActivationTest::RunTest(const FString&)
{
    auto State=MakeShared<FStarterSpellActivationState>();
    State->Instance.Reset(NewObject<UGameInstance>(GEngine));
    State->Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    State->World=State->Instance->GetWorld();
    if(!TestNotNull(TEXT("真实GI游戏世界"),State->World)) return false;
    State->World->InitializeActorsForPlay(FURL());
    State->Viewport.Reset(NewObject<UGameViewportClient>(GEngine));
    auto& Context=GEngine->GetWorldContextFromWorldChecked(State->World);
    Context.GameViewport=State->Viewport.Get();
    State->Viewport->Init(Context,State->Instance.Get(),false);
    State->SceneViewport=State->Viewport->CreateViewport(nullptr);
    FString Error;
    State->Player=State->Instance->CreateLocalPlayer(0,Error,false);
    if(!TestNotNull(TEXT("真实LocalPlayer：")+Error,State->Player.Get())) return false;
    State->Player->ViewportClient=State->Viewport.Get();
    TestEqual(TEXT("LocalPlayer关联真实世界"),State->Player->GetWorld(),State->World);
    TestEqual(TEXT("世界首位本地玩家"),State->Instance->GetFirstGamePlayer(),State->Player.Get());
    State->Service=State->Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>();
    if(!TestNotNull(TEXT("真实已初始化项目表现服务"),State->Service.Get()) ||
       !TestNotNull(TEXT("真实平台表现入口"),State->Service->PlatformPresentation)) return false;
    State->Service->EnsureStarterAudioForWorld(State->World);
    State->Pack=State->Service->StarterSpellPack;
    if(!TestTrue(TEXT("新增火球后法术包仍获得真实激活事务句柄"),State->Pack.IsValid())) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FWaitStarterSpellActivation(*this,State));
    return true;
}
#endif
