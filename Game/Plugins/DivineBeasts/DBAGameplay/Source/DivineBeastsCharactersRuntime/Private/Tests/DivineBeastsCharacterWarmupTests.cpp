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
#include "Engine/EngineBaseTypes.h"
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
        if (World) { World->EndPlay(EEndPlayReason::Quit); }
        Instance->Shutdown(); if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
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
            // GI dummy World仅完成基础初始化；真实Actor/组件Ready必须先走合法GameMode及BeginPlay，不能手改标志。
            FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
            if (!Test->TestTrue(TEXT("预热回归真实GameMode创建"), World->SetGameMode(URL))) { return true; }
            World->InitializeActorsForPlay(URL); World->BeginPlay();
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
        if (UWorld* OwnedWorld = Instance->GetWorld()) { OwnedWorld->Tick(LEVELTICK_All, 0.01f); }
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

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA
#include "DivineBeastsCharacterConfigurationTestFixture.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/EngineBaseTypes.h"

namespace
{
// 不制造Hero资产或私有Ready字段：真实Data成功租约→胶囊尺寸扩张→真实Overlap→同步后继/关闭/只注销Identity。
enum class ECharacterConfigurationOverlapAction : uint8 { BindSuccessor, EndIdentity, UnregisterIdentity };
class FCharacterConfigurationOverlapCommand final : public IAutomationLatentCommand
{
public:
    FCharacterConfigurationOverlapCommand(FAutomationTestBase* InTest, FName InHero, ECharacterConfigurationOverlapAction InAction)
        : Test(InTest), Hero(InHero), Action(InAction), Start(FPlatformTime::Seconds()) {}
    ~FCharacterConfigurationOverlapCommand() override
    {
        if (!Instance.IsValid()) { return; }
        if (Pawn.IsValid()) { Pawn->DisarmConfigurationOverlap(); Pawn->Destroy(); }
        if (OverlapActor.IsValid()) { OverlapActor->Destroy(); }
        if (auto* Data = IGamePlatformDataService::Get(*Instance))
        { if (WarmupLease.IsValid()) { Data->ReleaseResources(WarmupLease); } }
        UWorld* World = Instance->GetWorld();
        if (World) { World->EndPlay(EEndPlayReason::Quit); }
        Instance->Shutdown();
        if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Start > 30.0) { Test->AddError(TEXT("真实配置Overlap回归等待Hero超时")); return true; }
        if (!Instance.IsValid())
        {
            Path = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(Hero);
            if (!Path.IsValid()) { Test->AddError(TEXT("配置Overlap回归缺真实注册Hero主资产，不能伪造成功")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            UWorld* World = Instance->GetWorld();
            auto* Data = IGamePlatformDataService::Get(*Instance);
            if (!Test->TestNotNull(TEXT("真实GI世界"), World) || !Test->TestNotNull(TEXT("正式Data服务"), Data)) { return true; }
            // 用合法GameMode启动World/Physics，不直接写bBegunPlay；CreateWorld由InitializeStandalone内部只执行一次。
            FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
            if (!Test->TestTrue(TEXT("真实基础GameMode创建"), World->SetGameMode(URL))) { return true; }
            World->InitializeActorsForPlay(URL); World->BeginPlay();
            if (!Test->TestTrue(TEXT("真实World已BeginPlay且有物理场景"), World->HasBegunPlay() && World->GetPhysicsScene())) { return true; }
            WarmupOwner = World->SpawnActor<AActor>();
            if (!Test->TestTrue(TEXT("真实预热Owner"), WarmupOwner.IsValid())) { return true; }
            FGamePlatformResult Accepted;
            WarmupLease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, WarmupOwner.Get(),
                [WeakOwner = WarmupOwner](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
                { if (WeakOwner.IsValid() && !Result.IsSuccess()) { UE_LOG(LogTemp, Warning, TEXT("Configuration warmup failed: %s"), *Result.Code.ToString()); } }, Accepted);
            if (!Test->TestTrue(TEXT("真实预热需求受理"), Accepted.IsSuccess())) { return true; }
            return false;
        }
        UWorld* World = Instance->GetWorld();
        auto* Data = IGamePlatformDataService::Get(*Instance);
        if (!World || !Data) { Test->AddError(TEXT("配置回归世界或服务提前退出")); return true; }
        // Automation实际帧推进独立World，驱动正式Data next-tick完成；不伪造GFrameCounter或手动完成回调。
        World->Tick(LEVELTICK_All, 0.01f);
        const auto State = Data->GetLeaseState(WarmupLease);
        if (State == EGamePlatformDataRequestState::Loading) { return false; }
        if (!Test->TestEqual(TEXT("真实Hero预热成功"), State, EGamePlatformDataRequestState::Succeeded)) { return true; }
        auto* Definition = Cast<UDivineBeastsHeroDefinition>(Path.ResolveObject());
        FString Error;
        if (!Test->TestNotNull(TEXT("真实预热Hero定义"), Definition) ||
            !Test->TestTrue(TEXT("真实Hero定义有效"), Definition->IsProjectDefinitionValid(Error))) { return true; }
        FActorSpawnParameters SpawnParameters; SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ADivineBeastsCharacterConfigurationTestPawn>(SpawnParameters);
        auto* Controller = World->SpawnActor<APlayerController>();
        if (!Test->TestTrue(TEXT("真实配置Pawn"), Pawn.IsValid()) || !Test->TestNotNull(TEXT("真实Controller"), Controller)) { return true; }
        Controller->Possess(Pawn.Get());
        auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        auto* ASC = Pawn->GetGamePlatformAbilitySystemComponent();
        if (!Test->TestNotNull(TEXT("真实Identity"), Identity) || !Test->TestNotNull(TEXT("真实ASC"), ASC)) { return true; }
        FGamePlatformCharacterInitializationContext Context; Context.CharacterId = TEXT("Automation.Configuration.First");
        Context.HeroDefinitionId = Hero; Context.SpawnGeneration = 1; Context.AvatarGeneration = 1;
        if (!Test->TestTrue(TEXT("第一代可信身份受理"), Identity->AuthorityBindTrustedContext(Context, Error))) { return true; }
        auto* Capsule = Pawn->GetCapsuleComponent(); auto* Movement = Pawn->GetCharacterMovement();
        auto* Momentum = const_cast<UDivineBeastsMomentumAttributeSet*>(ASC->AddSet<UDivineBeastsMomentumAttributeSet>());
        if (!Test->TestNotNull(TEXT("真实Momentum属性集"), Momentum)) { return true; }
        Movement->DisableMovement();
        Capsule->SetCapsuleSize(Definition->SpawnEnvelope.CapsuleRadius * 0.1f, Definition->SpawnEnvelope.CapsuleHalfHeight, false);
        Capsule->SetCollisionProfileName(TEXT("OverlapAllDynamic"), false); Capsule->SetGenerateOverlapEvents(true);
        OverlapActor = World->SpawnActor<AActor>();
        if (!Test->TestTrue(TEXT("真实外置碰撞Actor"), OverlapActor.IsValid())) { return true; }
        auto* Sphere = NewObject<USphereComponent>(OverlapActor.Get());
        OverlapActor->AddInstanceComponent(Sphere); OverlapActor->SetRootComponent(Sphere);
        Sphere->SetSphereRadius(Definition->SpawnEnvelope.CapsuleRadius * 0.1f);
        Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
        Sphere->SetGenerateOverlapEvents(true); Sphere->RegisterComponent();
        OverlapActor->SetActorLocation(Pawn->GetActorLocation() + FVector(Definition->SpawnEnvelope.CapsuleRadius * 0.8f, 0, 0));
        if (!Test->TestTrue(TEXT("胶囊已有真实PhysicsState"), Capsule->IsPhysicsStateCreated()) ||
            !Test->TestFalse(TEXT("原小胶囊与Sphere尚未重叠"), Capsule->IsOverlappingComponent(Sphere))) { return true; }
        // 哨兵是后继消费者的实际副作用：旧Definition尾写会把它们覆盖，不依赖私有配置标志的镜像断言。
        bool bSuccessorBound = false; bool bSuccessorReady = false;
        const FGuid OriginalOperationId = Identity->GetTrustedContextOperationId();
        Pawn->ArmConfigurationOverlap(OverlapActor.Get(), [&]()
        {
            if (Action == ECharacterConfigurationOverlapAction::EndIdentity) { Identity->EndPlay(EEndPlayReason::Destroyed); }
            else if (Action == ECharacterConfigurationOverlapAction::UnregisterIdentity)
            {
                // 只调用真实引擎注销：不会EndPlay或换可信操作ID，Owner回退世界仍有效，正好隔离运行资格缺口。
                Identity->UnregisterComponent();
            }
            else
            {
                auto Successor = Context; Successor.CharacterId = TEXT("Automation.Configuration.Successor");
                Successor.SpawnGeneration = 2; Successor.AvatarGeneration = 2;
                FString SuccessorError;
                bSuccessorBound = Identity->AuthorityBindTrustedContext(Successor, SuccessorError);
                bSuccessorReady = Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), SuccessorError);
            }
            Capsule->SetCollisionProfileName(TEXT("OverlapAllDynamic"), false);
            Movement->MaxWalkSpeed = 1234.0f; Momentum->InitMaxMomentum(321.0f); Momentum->InitMomentum(123.0f);
        });
        Test->TestFalse(TEXT("原交接被真实重叠通知撤销"), Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), Error));
        Test->TestEqual(TEXT("真实物理重叠触发一次目标监听"), Pawn->GetConfigurationOverlapNotifications(), 1);
        Pawn->DisarmConfigurationOverlap();
        Test->TestEqual(TEXT("旧配置不覆盖监听者碰撞状态"), Capsule->GetCollisionProfileName(), FName(TEXT("OverlapAllDynamic")));
        Test->TestEqual(TEXT("旧配置不覆盖监听者移动状态"), Movement->MaxWalkSpeed, 1234.0f);
        Test->TestEqual(TEXT("旧配置不覆盖监听者Momentum上限"), Momentum->GetMaxMomentum(), 321.0f);
        Test->TestEqual(TEXT("旧配置不覆盖监听者Momentum值"), Momentum->GetMomentum(), 123.0f);
        if (Action == ECharacterConfigurationOverlapAction::EndIdentity)
        {
            Test->TestFalse(TEXT("关闭后Ready保持失败关闭"), Identity->IsCharacterReady());
            Test->TestNull(TEXT("关闭后不恢复旧加载定义"), Identity->GetLoadedDefinition());
        }
        else if (Action == ECharacterConfigurationOverlapAction::UnregisterIdentity)
        {
            Test->TestFalse(TEXT("Overlap确实只注销Identity"), Identity->IsRegistered());
            Test->TestTrue(TEXT("注销不调用EndPlay，原BeginPlay状态仍在"), Identity->HasBegunPlay());
            Test->TestEqual(TEXT("注销不更换可信绑定操作ID"), Identity->GetTrustedContextOperationId(), OriginalOperationId);
            Test->TestEqual(TEXT("注销后GetWorld仍可回退原Owner世界"), Identity->GetWorld(), World);
            Test->TestNotNull(TEXT("注销并未通过清定义模拟取消"), Identity->GetLoadedDefinition());
            Test->TestFalse(TEXT("注销后公开Ready拒绝"), Identity->IsCharacterReady());
            Test->TestFalse(TEXT("注销后公开激活门禁拒绝"), ASC->EvaluateActivationEligibility().IsSuccess());
        }
        else
        {
            Test->TestTrue(TEXT("真实后继绑定受理"), bSuccessorBound); Test->TestTrue(TEXT("真实后继预热配置成功"), bSuccessorReady);
            Test->TestEqual(TEXT("真实后继代次保持2"), Identity->GetAvatarGeneration(), 2);
            Test->TestTrue(TEXT("旧失败栈不清后继Ready"), Identity->IsCharacterReady());
        }
        return true;
    }
private:
    FAutomationTestBase* Test;
    FName Hero;
    ECharacterConfigurationOverlapAction Action;
    double Start;
    FSoftObjectPath Path;
    FGamePlatformDataLease WarmupLease;
    TStrongObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<AActor> WarmupOwner;
    TWeakObjectPtr<AActor> OverlapActor;
    TWeakObjectPtr<ADivineBeastsCharacterConfigurationTestPawn> Pawn;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterConfigurationSuccessorTest,
    "DivineBeasts.Characters.Configuration.ReentrantOverlapSuccessor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterConfigurationSuccessorTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString Hero;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DivineBeastsCharacterTestHero="), Hero))
    { AddError(TEXT("配置重入回归须指定真实已注册DivineBeastsCharacterTestHero")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCharacterConfigurationOverlapCommand(this, FName(*Hero), ECharacterConfigurationOverlapAction::BindSuccessor)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterConfigurationCloseTest,
    "DivineBeasts.Characters.Configuration.CloseDuringOverlap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterConfigurationCloseTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString Hero;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DivineBeastsCharacterTestHero="), Hero))
    { AddError(TEXT("配置关闭回归须指定真实已注册DivineBeastsCharacterTestHero")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCharacterConfigurationOverlapCommand(this, FName(*Hero), ECharacterConfigurationOverlapAction::EndIdentity)); return true;
}
// 独立注册真实注销回归；缺真实Hero/Physics前提必须失败，不能把源码断言视作UE通过。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterConfigurationUnregisterTest,
    "DivineBeasts.Characters.Configuration.UnregisterDuringOverlap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterConfigurationUnregisterTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString Hero;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DivineBeastsCharacterTestHero="), Hero))
    { AddError(TEXT("配置注销回归须指定真实已注册DivineBeastsCharacterTestHero")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCharacterConfigurationOverlapCommand(this, FName(*Hero), ECharacterConfigurationOverlapAction::UnregisterIdentity)); return true;
}
#endif
