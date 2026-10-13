// 项目适配生命周期回归：真实Actor销毁与平台同步通知，不加载Niagara、不冒充GPU或网络验收。
// Active账本及受理provider仅为同步重入夹具；生产内容包仍必须通过真实Data预载事务。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
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
    Service->UnbindDevelopmentMagicWorld();
    TestEqual(TEXT("解绑后所有自有条目撤销"),Service->DevelopmentMagicFlights.Num(),0);
    Platform->UnregisterProvider(Provider);
    World->DestroyWorld(false);
    return true;
}
#endif
