// 真实资源预热交接回归：必须使用已生成/注册Hero资产及正式Data服务，不制作资产替身、不把缺前提记为通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"
namespace
{
class FCharacterWarmupCommand final : public IAutomationLatentCommand
{
public:
    FCharacterWarmupCommand(FAutomationTestBase* InTest, FName InHero) : Test(InTest), Hero(InHero), Start(FPlatformTime::Seconds()) {}
    ~FCharacterWarmupCommand() override
    {
        if (!Instance.IsValid()) { return; }
        // 先销毁借用者，再释放测试装配预热需求，最后结束GI；没有卸载其他测试/世界的资源。
        if (Pawn.IsValid()) { Pawn->Destroy(); }
        auto* Data = IGamePlatformDataService::Get(*Instance);
        if (Data && WarmupLease.IsValid()) { Data->ReleaseResources(WarmupLease); }
        UWorld* World = Instance->GetWorld();
        if (World) { World->DestroyWorld(false); }
        Instance->Shutdown(); if (World) { GEngine->DestroyWorldContext(World); }
        if (OtherWorld.IsValid()) { OtherWorld->DestroyWorld(false); }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Start > 30.0) { Test->AddError(TEXT("真实Hero预热交接超时")); return true; }
        if (!Instance.IsValid())
        {
            Path = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(Hero);
            if (!Path.IsValid()) { Test->AddError(TEXT("缺少真实已注册Hero主资产，不能执行预热成功用例")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            UWorld* World = Instance->GetWorld(); auto* Data = IGamePlatformDataService::Get(*Instance);
            if (!World || !Data) { Test->AddError(TEXT("真实GameInstance/World/Data服务未创建")); return true; }
            WarmupOwner = World->SpawnActor<AActor>();
            FGamePlatformResult Accepted;
            WarmupLease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, WarmupOwner,
                [WeakOwner = WarmupOwner](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
                { if (WeakOwner.IsValid() && !Result.IsSuccess()) { UE_LOG(LogTemp, Warning, TEXT("Automation warmup failed: %s"), *Result.Code.ToString()); } }, Accepted);
            Test->TestTrue(TEXT("真实预热需求受理"), Accepted.IsSuccess());
            Pawn = World->SpawnActor<ADivineBeastsCharacter>();
            auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Pawn.Get()); Pawn->DispatchBeginPlay();
            FGamePlatformCharacterInitializationContext Context; Context.CharacterId = TEXT("Automation.Character");
            Context.HeroDefinitionId = Hero; Context.SpawnGeneration = 1; Context.AvatarGeneration = 1;
            FString Error;
            auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
            Test->TestTrue(TEXT("真实角色需求受理"), Identity->AuthorityBindTrustedContext(Context, Error));
            Test->TestFalse(TEXT("冷Loading租约不能交接为Ready"), Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner, Error));
            Pawn->Destroy(); Pawn.Reset(); return false;
        }
        auto* Data = IGamePlatformDataService::Get(*Instance);
        const auto State = Data->GetLeaseState(WarmupLease);
        if (State == EGamePlatformDataRequestState::Loading) { return false; }
        if (State != EGamePlatformDataRequestState::Succeeded) { Test->AddError(TEXT("真实Hero预热没有Succeeded")); return true; }
        UWorld* World = Instance->GetWorld();
        Pawn = World->SpawnActor<ADivineBeastsCharacter>();
        auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Pawn.Get()); Pawn->DispatchBeginPlay();
        FGamePlatformCharacterInitializationContext Context; Context.CharacterId = TEXT("Automation.Character");
        Context.HeroDefinitionId = Hero; Context.SpawnGeneration = 2; Context.AvatarGeneration = 2;
        FString Error; auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        Test->TestTrue(TEXT("新Pawn自身需求真实受理"), Identity->AuthorityBindTrustedContext(Context, Error));
        // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
        const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        OtherWorld.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &WorldInitializationValues));
        auto* WrongWorldOwner = OtherWorld->SpawnActor<AActor>();
        Test->TestFalse(TEXT("旧/其他世界Owner拒绝"), Identity->TryUsePreloadedDefinition(WarmupLease, *WrongWorldOwner, Error));
        auto WrongHeroLease = WarmupLease; WrongHeroLease.ResourcePaths = {FSoftObjectPath(TEXT("/Game/WrongHero.WrongHero"))};
        Test->TestFalse(TEXT("错误Hero需求拒绝"), Identity->TryUsePreloadedDefinition(WrongHeroLease, *WarmupOwner, Error));
        Test->TestTrue(TEXT("同世界真实Succeeded预热交接"), Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner, Error));
        Test->TestTrue(TEXT("自己的NextTick加载前依靠真实借用需求Ready"), Identity->IsCharacterReady());
        Pawn->Destroy(); Pawn.Reset();
        Test->TestEqual(TEXT("借用者销毁不释放装配需求"), Data->GetLeaseState(WarmupLease), EGamePlatformDataRequestState::Succeeded);
        return true;
    }
private:
    FAutomationTestBase* Test;
    FName Hero;
    double Start;
    FSoftObjectPath Path;
    FGamePlatformDataLease WarmupLease;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UWorld> OtherWorld;
    TWeakObjectPtr<AActor> WarmupOwner;
    TWeakObjectPtr<ADivineBeastsCharacter> Pawn;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterWarmupTest, "DivineBeasts.Character.Pawn.VerifiedWarmupHandoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterWarmupTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString Hero;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DivineBeastsCharacterTestHero="), Hero))
    { AddError(TEXT("须指定真实已注册DivineBeastsCharacterTestHero，缺前提不能假通过")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCharacterWarmupCommand(this, FName(*Hero)));
    return true;
}
#endif
