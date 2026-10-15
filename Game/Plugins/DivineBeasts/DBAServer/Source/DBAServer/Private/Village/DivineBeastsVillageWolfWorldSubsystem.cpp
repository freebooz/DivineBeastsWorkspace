// SSLM狼怪物专服装配：客户端只接收Actor移动/生命及AI状态，不执行生成、攻击或独立PCG决定。
#include "Village/DivineBeastsVillageWolfWorldSubsystem.h"

#include "Village/DivineBeastsVillageWolfAIController.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GamePlatformGameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"

namespace
{
    /** 小规模新手村专服限额，未来经项目怪物生成Definition覆盖；单位：厘米/秒。 */
    constexpr int32 MaxVillageWolves = 4;
    constexpr float PlayerStartSafetyRadiusCm = 2200.0f;
    constexpr float MinimumWolfSeparationCm = 500.0f;
    constexpr float WolfCorpseLifetimeSeconds = 4.0f;
    constexpr float WolfRespawnDelaySeconds = 14.0f;
}

bool UDivineBeastsVillageWolfWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game || Type == EWorldType::PIE; }

bool UDivineBeastsVillageWolfWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && !IsRunningCommandlet() && !bClosing && DoesSupportWorldType(World->WorldType) &&
        (IsRunningDedicatedServer() || World->GetNetMode() == NM_DedicatedServer) &&
        Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsVillageWolfWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!ShouldCreateSubsystem(GetWorld())) return;
    WorldOperation = FGuid::NewGuid();
    if (const UWorld* World = GetWorld())
    {
        if (UGameInstance* Instance = World->GetGameInstance())
        {
            auto* Lifecycle = Instance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
            ObservedLifecycle = Lifecycle;
            if (Lifecycle) LifecycleSubscription = Lifecycle->OnLifecycleChanged().AddUObject(
                this, &ThisClass::HandleLifecycleChanged);
        }
    }
}

void UDivineBeastsVillageWolfWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (&InWorld != GetWorld() || bClosing || !WorldOperation.IsValid()) return;
    ObserveExperience();
    TryFillWolves();
}

bool UDivineBeastsVillageWolfWorldSubsystem::HasVerifiedAnimalAssets() const
{
    if (!GConfig) return false;
    bool bEnabled = false, bClientAssetsVerified = false;
    const TCHAR* Section = TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section, TEXT("bEnableVillageWolves"), bEnabled, GGameIni);
    GConfig->GetBool(Section, TEXT("bClientVisualsVerified"), bClientAssetsVerified, GGameIni);
    FString DefinitionPath;
    GConfig->GetString(Section, TEXT("WolfAIDefinition"), DefinitionPath, GGameIni);
    const FSoftObjectPath Definition(DefinitionPath);
    // 编辑器之外只检查权威AI定义所在的小型服务器Cook包；不访问客户端Fab资产包。
    return bEnabled && bClientAssetsVerified && Definition.IsValid() &&
        FPackageName::IsValidLongPackageName(Definition.GetLongPackageName()) &&
        FPackageName::DoesPackageExist(Definition.GetLongPackageName());
}

bool UDivineBeastsVillageWolfWorldSubsystem::CaptureApprovedScope(FApprovedScope& OutScope) const
{
    OutScope = {};
    const UWorld* World = GetWorld();
    if (!IsInGameThread() || bClosing || !WorldOperation.IsValid() || !HasVerifiedAnimalAssets() ||
        !World || !DoesSupportWorldType(World->WorldType) || !World->HasBegunPlay() || World->bIsTearingDown ||
        World->GetNetMode() != NM_DedicatedServer || !World->GetAuthGameMode()) return false;
    UGameInstance* Instance = World->GetGameInstance();
    auto* Bootstrap = Instance ? Instance->GetSubsystem<UDivineBeastsServerBootstrapSubsystem>() : nullptr;
    const auto* Lifecycle = ObservedLifecycle.Get();
    const auto* Experience = ObservedExperience.Get();
    // 平台GetReadiness接口非const；保留服务原始接口类型，不改变本桥的只读资格检查职责。
    auto* Service = IGamePlatformWorldService::Get(*const_cast<UWorld*>(World));
    if (!Bootstrap || !Lifecycle || !Experience || !Service ||
        Experience->GetWorld() != World || Lifecycle->GetGameInstance() != Instance) return false;
    const auto State = Bootstrap->GetBootstrapState();
    if (State == EDivineBeastsServerBootstrapState::Draining ||
        State == EDivineBeastsServerBootstrapState::Stopped ||
        State == EDivineBeastsServerBootstrapState::Failed) return false;
    const auto* Profile = Bootstrap->GetActiveProfile();
    const auto Life = Lifecycle->GetSnapshot();
    const auto Active = Experience->GetExperienceSnapshot();
    const auto Ready = Service->GetReadiness();
    FGamePlatformId Tutorial, ServerExperience;
    if (!Profile || Life.State != EGamePlatformServerLifecycleState::Ready ||
        !FGamePlatformId::TryParse(FDivineBeastsProjectCatalog::GetVillageTutorialExperience().ToString() + TEXT("@1"), Tutorial) ||
        !FGamePlatformId::TryParse(Life.ExperienceId + TEXT("@1"), ServerExperience) ||
        Profile->ServerRoleId != FDivineBeastsProjectCatalog::GetVillageServerRole() ||
        FName(*Life.ServerRoleId) != Profile->ServerRoleId ||
        Active.ExperienceId != Tutorial || ServerExperience != Tutorial || !Active.IsServerActive() ||
        Ready.Context.ReadinessState != EGamePlatformWorldReadiness::Ready ||
        Ready.Context.ServerRole != FName(TEXT("Village")) || Ready.Context.ExperienceId != Active.ExperienceId ||
        Ready.Context.MapPackageName != Profile->WorldPackage || !Ready.Context.ContextGeneration.IsValid() ||
        Ready.Context.ContextGeneration != Active.WorldContextGeneration) return false;
    OutScope.WorldGeneration = Active.WorldContextGeneration;
    OutScope.ExperienceEpoch = Active.ExperienceEpoch;
    return true;
}

void UDivineBeastsVillageWolfWorldSubsystem::ObserveExperience()
{
    if (bClosing || !WorldOperation.IsValid() || ObservedExperience.IsValid()) return;
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGamePlatformGameModeBase>() : nullptr;
    auto* Experience = Mode ? Mode->GetExperienceComponent() : nullptr;
    if (!Experience) return;
    const FGuid Operation = WorldOperation;
    ObservedExperience = Experience;
    FGamePlatformResult Result;
    const auto Subscription = Experience->SubscribeExperienceChanged(this,
        [WeakThis = TWeakObjectPtr<ThisClass>(this), Operation](const FGamePlatformExperienceSnapshot& Snapshot)
        {
            if (WeakThis.IsValid() && !WeakThis->bClosing && WeakThis->WorldOperation == Operation)
                WeakThis->HandleExperienceChanged(Snapshot);
        }, Result);
    if (bClosing || WorldOperation != Operation)
    {
        Experience->Unsubscribe(Subscription);
        return;
    }
    if (Result.IsSuccess()) ExperienceSubscription = Subscription;
    else ObservedExperience.Reset();
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot& Snapshot)
{
    if (bClosing) return;
    if (Snapshot.State == EGamePlatformServerLifecycleState::Draining ||
        Snapshot.State == EGamePlatformServerLifecycleState::Stopped ||
        Snapshot.State == EGamePlatformServerLifecycleState::Failed)
    { CloseScope(); return; }
    ObserveExperience();
    TryFillWolves();
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleExperienceChanged(const FGamePlatformExperienceSnapshot& Snapshot)
{
    if (bClosing) return;
    if (Snapshot.Stage == EGamePlatformExperienceStage::Draining ||
        Snapshot.Stage == EGamePlatformExperienceStage::Released ||
        Snapshot.Stage == EGamePlatformExperienceStage::Failed ||
        (OwnedScope.WorldGeneration.IsValid() &&
          (OwnedScope.WorldGeneration != Snapshot.WorldContextGeneration ||
           OwnedScope.ExperienceEpoch != Snapshot.ExperienceEpoch)))
    { CloseScope(); return; }
    TryFillWolves();
}

bool UDivineBeastsVillageWolfWorldSubsystem::FindSpawnTransform(int32 SpawnOrdinal, FTransform& OutTransform) const
{
    UWorld* World = GetWorld();
    auto* Nav = IsValid(World) ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    const auto* Capsule = GetDefault<ADivineBeastsVillageWolf>()->GetCapsuleComponent();
    if (!World || World->bIsTearingDown || !Nav || !Capsule) return false;

    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetPathName() < B.GetPathName(); });
    if (Starts.IsEmpty()) return false;

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Query(TEXT("VillageWolfSafeSpawn"), false);
    FCollisionObjectQueryParams Occupancy;
    Occupancy.AddObjectTypesToQuery(ECC_WorldStatic);
    Occupancy.AddObjectTypesToQuery(ECC_WorldDynamic);
    Occupancy.AddObjectTypesToQuery(ECC_Pawn);
    Occupancy.AddObjectTypesToQuery(ECC_PhysicsBody);

    // 只在PlayerStart外圈区域投影到真实NavMesh，不生成在湖水、屋内、阻挡物或不可达悬崖。
    for (const auto* Start : Starts)
    for (int32 Ring = 0; Ring < 3; ++Ring)
    for (int32 Angle = 0; Angle < 12; ++Angle)
    {
        const float Theta = 2.0f * PI * ((Angle + SpawnOrdinal * 3) % 12) / 12.0f;
        const float Distance = 2600.0f + 800.0f * Ring;
        const FVector Candidate = Start->GetActorLocation() +
            FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0) * Distance;
        FNavLocation NavPoint;
        if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(130.0f, 130.0f, 550.0f)))
            continue;

        bool bSafe = true;
        for (const APlayerStart* AnyStart : Starts)
            if (FVector::DistSquared2D(AnyStart->GetActorLocation(), NavPoint.Location) <
                FMath::Square(PlayerStartSafetyRadiusCm)) { bSafe = false; break; }
        for (const auto& OtherReference : OwnedWolves)
        {
            const auto* Other = OtherReference.Get();
            if (IsValid(Other) && !Other->IsActorBeingDestroyed() &&
                FVector::DistSquared2D(Other->GetActorLocation(), NavPoint.Location) <
                FMath::Square(MinimumWolfSeparationCm)) { bSafe = false; break; }
        }
        if (!bSafe) continue;

        FHitResult Ground;
        if (!World->LineTraceSingleByChannel(Ground, NavPoint.Location + FVector(0,0,260),
            NavPoint.Location - FVector(0,0,650), ECC_Visibility, Query) ||
            !Ground.bBlockingHit || Ground.ImpactNormal.Z < GetDefault<ADivineBeastsVillageWolf>()->GetCharacterMovement()->GetWalkableFloorZ())
            continue;
        const FVector Center = Ground.ImpactPoint + FVector(0,0,HalfHeight + 4);
        if (World->OverlapAnyTestByObjectType(Center,FQuat::Identity,Occupancy,
            FCollisionShape::MakeCapsule(Radius,HalfHeight),Query)) continue;
        OutTransform = FTransform(FRotator(0, FMath::RadiansToDegrees(Theta) + 180.0f,0),Center);
        return true;
    }
    return false;
}

bool UDivineBeastsVillageWolfWorldSubsystem::SpawnOneWolf(const FTransform& SpawnAt)
{
    UWorld* World = GetWorld();
    FApprovedScope Original;
    if (!World || bClosing || !CaptureApprovedScope(Original)) return false;
    FString DefinitionPath;
    if (!GConfig || !GConfig->GetString(TEXT("DivineBeasts.VillageWolves"),
        TEXT("WolfAIDefinition"),DefinitionPath,GGameIni)) return false;
    const FSoftObjectPath SoftDefinition(DefinitionPath);
    if (!SoftDefinition.IsValid()) return false;

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    ADivineBeastsVillageWolf* Wolf =
        World->SpawnActor<ADivineBeastsVillageWolf>(SpawnAt.GetLocation(),SpawnAt.Rotator(),Spawn);
    if (!IsValid(Wolf)) return false;
    Wolf->GetWolfAIState()->SetDefinitionAsset(
        TSoftObjectPtr<UGamePlatformAIDefinition>(SoftDefinition));
    auto* Controller=World->SpawnActor<ADivineBeastsVillageWolfAIController>(
        SpawnAt.GetLocation(),SpawnAt.Rotator());
    if (IsValid(Controller)) Controller->Possess(Wolf);
    FApprovedScope Current;
    if (!IsValid(Controller) || !IsValid(Wolf) || Wolf->GetController()!=Controller ||
        !Wolf->AuthorityInitializeWolf(75.0f) || !CaptureApprovedScope(Current) || !(Current == Original))
    {
        // 没有完整人工验收资产、导航、控制器、生命或World资格就拒绝产出半可交互敌人。
        if (IsValid(Controller)) Controller->Destroy();
        if (IsValid(Wolf)) Wolf->Destroy();
        return false;
    }
    OwnedWolves.Add(Wolf);
    Wolf->OnAuthoritativeDeath().AddUObject(this,&ThisClass::HandleWolfDeath);
    Wolf->OnDestroyed.AddDynamic(this,&ThisClass::HandleWolfDestroyed);
    OwnedScope=Original;
    UE_LOG(LogTemp,Log,TEXT("VillageWolfSpawned: 专服资格与Combat初始化成功；AI定义/客户端外观仍按租约异步加载，LocationCm=%s"),
        *Wolf->GetActorLocation().ToString());
    return true;
}

void UDivineBeastsVillageWolfWorldSubsystem::TryFillWolves()
{
    if (bClosing || bCreating) return;
    FApprovedScope Scope;
    if (!CaptureApprovedScope(Scope)) return;
    if (OwnedScope.WorldGeneration.IsValid() && !(OwnedScope == Scope))
    { CloseScope(); return; }
    bCreating=true;
    // 死亡尸体不算活跃怪；只移除已销毁的弱引用，未完成死亡退场的仍占上限。
    OwnedWolves.RemoveAll([](const auto& Wolf) { return !IsValid(Wolf); });
    for (int32 i=OwnedWolves.Num();i<MaxVillageWolves && !bClosing;++i)
    {
        FTransform SpawnAt;
        if (!FindSpawnTransform(i,SpawnAt) || !SpawnOneWolf(SpawnAt)) break;
    }
    bCreating=false;
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleWolfDeath(const FGamePlatformCombatEvent& Event)
{
    auto* Wolf=Cast<ADivineBeastsVillageWolf>(Event.TargetActor.Get());
    if (bClosing || !IsValid(Wolf) || !OwnedWolves.Contains(Wolf) ||
        Event.EventType!=EGamePlatformCombatEventType::Death || !Wolf->GetGamePlatformCombatComponent()->IsCombatDead()) return;
    Wolf->SetLifeSpan(WolfCorpseLifetimeSeconds);
    FApprovedScope Scope;
    if (!CaptureApprovedScope(Scope) || !(Scope==OwnedScope)) {CloseScope();return;}
    const FGuid Ticket=FGuid::NewGuid();
    RespawnTicket=Ticket;
    GetWorld()->GetTimerManager().SetTimer(ReplenishTimer,
        FTimerDelegate::CreateWeakLambda(this,[this,Ticket]{ReplenishWolves(Ticket);}),
        WolfRespawnDelaySeconds,false);
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleWolfDestroyed(AActor* DestroyedActor)
{
    if (bClosing) return;
    auto* Wolf=Cast<ADivineBeastsVillageWolf>(DestroyedActor);
    if (!Wolf || !OwnedWolves.Contains(Wolf)) return;
    Wolf->OnAuthoritativeDeath().RemoveAll(this);
    Wolf->OnDestroyed.RemoveDynamic(this,&ThisClass::HandleWolfDestroyed);
    OwnedWolves.Remove(Wolf);
    // 意外销毁也按固定延迟恢复，避免攻击目标离线或缺导航造成紧密刷怪风暴。
    FApprovedScope Scope;
    if (!CaptureApprovedScope(Scope) || !(Scope==OwnedScope)) {CloseScope();return;}
    if (!GetWorld()->GetTimerManager().IsTimerActive(ReplenishTimer))
    {
        const FGuid Ticket=FGuid::NewGuid();
        RespawnTicket=Ticket;
        GetWorld()->GetTimerManager().SetTimer(ReplenishTimer,
            FTimerDelegate::CreateWeakLambda(this,[this,Ticket]{ReplenishWolves(Ticket);}),
            WolfRespawnDelaySeconds,false);
    }
}

void UDivineBeastsVillageWolfWorldSubsystem::ReplenishWolves(FGuid Ticket)
{
    if (bClosing || Ticket!=RespawnTicket) return;
    RespawnTicket.Invalidate();
    GetWorld()->GetTimerManager().ClearTimer(ReplenishTimer);
    TryFillWolves();
}

void UDivineBeastsVillageWolfWorldSubsystem::CloseScope()
{
    if (bClosing) return;
    bClosing=true;
    WorldOperation.Invalidate();
    RespawnTicket.Invalidate();
    if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(ReplenishTimer);
    if (auto* Exp=ObservedExperience.Get()) Exp->Unsubscribe(ExperienceSubscription);
    ExperienceSubscription={};
    ObservedExperience.Reset();
    if (auto* Life=ObservedLifecycle.Get()) Life->OnLifecycleChanged().Remove(LifecycleSubscription);
    LifecycleSubscription.Reset();
    ObservedLifecycle.Reset();
    TArray<TObjectPtr<ADivineBeastsVillageWolf>> ToDestroy=MoveTemp(OwnedWolves);
    OwnedWolves.Reset();
    for (const auto& WolfReference:ToDestroy)
    {
        auto* Wolf=WolfReference.Get();
        if (!IsValid(Wolf)) continue;
        Wolf->OnAuthoritativeDeath().RemoveAll(this);
        Wolf->OnDestroyed.RemoveDynamic(this,&ThisClass::HandleWolfDestroyed);
        AController* Controller=Wolf->GetController();
        if (!Wolf->IsActorBeingDestroyed() && Wolf->HasAuthority()) Wolf->Destroy();
        if (IsValid(Controller) && !Controller->IsActorBeingDestroyed()) Controller->Destroy();
    }
}

void UDivineBeastsVillageWolfWorldSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
    if (&InWorld==GetWorld()) CloseScope();
    Super::OnWorldEndPlay(InWorld);
}

void UDivineBeastsVillageWolfWorldSubsystem::Deinitialize()
{
    CloseScope();
    Super::Deinitialize();
}
