// 项目开发专服装配：使用角色唯一开发守卫及实际平台世界/体验事实；不轮询、不联网、不加载表现。
#include "Development/DivineBeastsDevelopmentMonsterWorldSubsystem.h"

#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GamePlatformGameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"
#include "UObject/StrongObjectPtr.h"

bool UDivineBeastsDevelopmentMonsterWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{ return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

bool UDivineBeastsDevelopmentMonsterWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    // Dedicated进程的NetDriver可能尚未建立；实际生成路径仍重新要求NM_DedicatedServer和真实AuthGameMode。
    return World && !IsRunningCommandlet() && !bClosing &&
        ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess() &&
        (IsRunningDedicatedServer() || World->GetNetMode() == NM_DedicatedServer) &&
        Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (bClosing || !ShouldCreateSubsystem(GetWorld())) return;
    WorldOperationId = FGuid::NewGuid();
    if (auto* Instance = GetWorld()->GetGameInstance())
    {
        auto* Lifecycle = Instance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
        ObservedLifecycle = Lifecycle;
        if (Lifecycle)
            LifecycleSubscription = Lifecycle->OnLifecycleChanged().AddUObject(this, &ThisClass::HandleLifecycleChanged);
    }
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (&InWorld != GetWorld() || bClosing || !WorldOperationId.IsValid()) return;
    ObserveExperience();
    TryCreateOwnedMonster();
}

bool UDivineBeastsDevelopmentMonsterWorldSubsystem::IsVillageTutorialIdentity(
    FName ServerRoleId, const FGamePlatformId& ExperienceId)
{
    FGamePlatformId Tutorial;
    return ServerRoleId == FDivineBeastsProjectCatalog::GetVillageServerRole() &&
        FGamePlatformId::TryParse(FDivineBeastsProjectCatalog::GetVillageTutorialExperience().ToString() + TEXT("@1"), Tutorial) &&
        ExperienceId.IsValid() && ExperienceId == Tutorial;
}

bool UDivineBeastsDevelopmentMonsterWorldSubsystem::CaptureApprovedWorldScope(FApprovedWorldScope& OutScope) const
{
    OutScope = {};
    UWorld* World = GetWorld();
    if (!IsInGameThread() || bClosing || !WorldOperationId.IsValid() ||
        !ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess() ||
        !World || !DoesSupportWorldType(World->WorldType) || !World->HasBegunPlay() || World->bIsTearingDown ||
        World->GetNetMode() != NM_DedicatedServer || !World->GetAuthGameMode()) return false;
    auto* Instance = World->GetGameInstance();
    auto* Bootstrap = Instance ? Instance->GetSubsystem<UDivineBeastsServerBootstrapSubsystem>() : nullptr;
    auto* Lifecycle = ObservedLifecycle.Get();
    auto* Experience = ObservedExperience.Get();
    auto* Service = IGamePlatformWorldService::Get(*World);
    if (!Bootstrap || !Lifecycle || !Experience || !Service ||
        Experience->GetWorld() != World || Lifecycle->GetGameInstance() != Instance) return false;
    const auto BootstrapState = Bootstrap->GetBootstrapState();
    if (BootstrapState == EDivineBeastsServerBootstrapState::Draining ||
        BootstrapState == EDivineBeastsServerBootstrapState::Stopped ||
        BootstrapState == EDivineBeastsServerBootstrapState::Failed) return false;
    const auto* Profile = Bootstrap->GetActiveProfile();
    const auto Server = Lifecycle->GetSnapshot();
    const auto Active = Experience->GetExperienceSnapshot();
    const auto Ready = Service->GetReadiness();
    FGamePlatformId ServerExperience;
    if (!Profile || Server.State != EGamePlatformServerLifecycleState::Ready ||
        !FGamePlatformId::TryParse(Server.ExperienceId + TEXT("@1"), ServerExperience) ||
        !IsVillageTutorialIdentity(Profile->ServerRoleId, Active.ExperienceId) ||
        !IsVillageTutorialIdentity(FName(*Server.ServerRoleId), ServerExperience) ||
        !Active.IsServerActive() || Ready.Context.ReadinessState != EGamePlatformWorldReadiness::Ready ||
        Ready.Context.ServerRole != FName(TEXT("Village")) || Ready.Context.ExperienceId != Active.ExperienceId ||
        Ready.Context.MapPackageName != Profile->WorldPackage ||
        !Ready.Context.ContextGeneration.IsValid() ||
        Ready.Context.ContextGeneration != Active.WorldContextGeneration) return false;
    OutScope.WorldContextGeneration = Active.WorldContextGeneration;
    OutScope.ExperienceEpoch = Active.ExperienceEpoch;
    return true;
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::ObserveExperience()
{
    if (bClosing || !WorldOperationId.IsValid() || ObservedExperience.IsValid()) return;
    auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AGamePlatformGameModeBase>() : nullptr;
    auto* Experience = Mode ? Mode->GetExperienceComponent() : nullptr;
    if (!Experience) return;
    const FGuid Operation = WorldOperationId;
    ObservedExperience = Experience;
    FGamePlatformResult Result;
    const auto Subscription = Experience->SubscribeExperienceChanged(this,
        [WeakThis = TWeakObjectPtr<ThisClass>(this), Operation](const FGamePlatformExperienceSnapshot& Snapshot)
        {
            if (WeakThis.IsValid() && !WeakThis->bClosing && WeakThis->WorldOperationId == Operation)
                WeakThis->HandleExperienceChanged(Snapshot);
        }, Result);
    // 订阅可同步发布当前状态；关闭先撤本次返回句柄，不能把它尾写到已退休作用域。
    if (bClosing || WorldOperationId != Operation)
    {
        Experience->Unsubscribe(Subscription);
        return;
    }
    if (Result.IsSuccess()) ExperienceSubscription = Subscription;
    else ObservedExperience.Reset();
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot& Snapshot)
{
    if (bClosing) return;
    if (Snapshot.State == EGamePlatformServerLifecycleState::Draining ||
        Snapshot.State == EGamePlatformServerLifecycleState::Stopped || Snapshot.State == EGamePlatformServerLifecycleState::Failed)
    { CloseOwnedScope(); return; }
    ObserveExperience();
    TryCreateOwnedMonster();
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleExperienceChanged(const FGamePlatformExperienceSnapshot& Snapshot)
{
    if (bClosing) return;
    if (Snapshot.Stage == EGamePlatformExperienceStage::Draining ||
        Snapshot.Stage == EGamePlatformExperienceStage::Released || Snapshot.Stage == EGamePlatformExperienceStage::Failed ||
        (OwnedMonster && (Snapshot.WorldContextGeneration != OwnedWorldScope.WorldContextGeneration ||
            Snapshot.ExperienceEpoch != OwnedWorldScope.ExperienceEpoch)))
    { CloseOwnedScope(); return; }
    TryCreateOwnedMonster();
}

bool UDivineBeastsDevelopmentMonsterWorldSubsystem::FindSafeSpawnTransform(
    FTransform& OutTransform, const AActor* IgnoredActor) const
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown) return false;
    const auto* Prototype = GetDefault<ADivineBeastsDevelopmentMonster>();
    const auto* Capsule = Prototype->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) Starts.Add(*It);
    // 稳定只读次序便于开发复核，不注册/保留玩家出生点，不消费玩家SpawnPolicy候选。
    Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetPathName() < B.GetPathName(); });
    FCollisionQueryParams Query(TEXT("DivineBeastsDevelopmentMonsterSpawn"), false);
    if (IgnoredActor) Query.AddIgnoredActor(IgnoredActor);
    FCollisionObjectQueryParams Occupancy;
    Occupancy.AddObjectTypesToQuery(ECC_WorldStatic);
    Occupancy.AddObjectTypesToQuery(ECC_WorldDynamic);
    Occupancy.AddObjectTypesToQuery(ECC_Pawn);
    Occupancy.AddObjectTypesToQuery(ECC_PhysicsBody);
    for (const APlayerStart* Start : Starts)
    {
        FVector Forward = Start->GetActorForwardVector();
        Forward.Z = 0.0f;
        Forward = Forward.GetSafeNormal();
        if (Forward.IsNearlyZero()) continue;
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
        // 开发白盒候选距离单位厘米；只查询实际地面，不根据效果图或固定标高替换用户PlayerStart。
        for (float DistanceCm : {400.0f, 600.0f, 800.0f})
        for (float LateralCm : {0.0f, -150.0f, 150.0f})
        {
            const FVector Candidate = Start->GetActorLocation() + Forward * DistanceCm + Right * LateralCm;
            FHitResult Ground;
            if (!World->LineTraceSingleByChannel(Ground, Candidate + FVector(0, 0, 300),
                Candidate - FVector(0, 0, 1000), ECC_Visibility, Query) || !Ground.bBlockingHit ||
                Ground.ImpactNormal.Z < Prototype->GetCharacterMovement()->GetWalkableFloorZ()) continue;
            const FVector Center = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 2.0f);
            // 对物体通道保守拒绝任何实际占位；不能仅依赖Visibility命中或Spawn时单次碰撞调整。
            if (World->OverlapAnyTestByObjectType(Center, FQuat::Identity, Occupancy,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
            OutTransform = FTransform(FRotator(0, Forward.Rotation().Yaw + 180.0f, 0), Center);
            return true;
        }
    }
    return false;
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::TryCreateOwnedMonster()
{
    if (bClosing || bCreatingMonster || OwnedMonster) return;
    FApprovedWorldScope Scope;
    if (!CaptureApprovedWorldScope(Scope)) return;
    FTransform Transform;
    if (!FindSafeSpawnTransform(Transform))
    {
        UE_LOG(LogTemp, Warning, TEXT("DevelopmentMonsterSpawnRejected: 无PlayerStart前方安全地面/胶囊候选。"));
        return;
    }
    const FGuid Operation = WorldOperationId;
    bCreatingMonster = true;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    auto* Monster = GetWorld()->SpawnActor<ADivineBeastsDevelopmentMonster>(Transform.GetLocation(), Transform.Rotator(), Spawn);
    FApprovedWorldScope Current;
    if (bClosing || WorldOperationId != Operation || !CaptureApprovedWorldScope(Current) || !(Current == Scope))
    {
        // 只有仍属于本操作才撤生成中标记；退出已单向封闭，不能恢复后继作用域。
        if (!bClosing && WorldOperationId == Operation) bCreatingMonster = false;
        if (IsValid(Monster)) Monster->Destroy();
        return;
    }
    bCreatingMonster = false;
    if (!Monster) return;
    OwnedMonster = Monster;
    OwnedWorldScope = Scope;
    DeathSubscription = Monster->OnAuthoritativeDeath().AddUObject(this, &ThisClass::HandleOwnedMonsterDeath);
    Monster->OnDestroyed.AddDynamic(this, &ThisClass::HandleOwnedMonsterDestroyed);
    const bool bInitialized = Monster->AuthorityInitializeDevelopmentMonster();
    if (bClosing || WorldOperationId != Operation) return;
    if (!bInitialized || !CaptureApprovedWorldScope(Current) || !(Current == Scope))
    {
        UE_LOG(LogTemp, Warning, TEXT("DevelopmentMonsterSpawnRejected: 真实初始化或世界资格失效。"));
        CloseOwnedScope();
        return;
    }
    // 一次成功证据必须在实际Combat生命和世界资格提交后记录；不把Spawn非空当成就绪。
    const auto Life = Monster->GetLifeState();
    UE_LOG(LogTemp, Log, TEXT("DevelopmentMonsterReady Generation=%d Health=%.0f MaxHealth=%.0f LocationCm=%s"),
        Life.AvatarGeneration, Life.Health, Life.MaxHealth, *Monster->GetActorLocation().ToString());
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleOwnedMonsterDeath(const FGamePlatformCombatEvent& Event)
{
    auto* Monster = OwnedMonster.Get();
    const auto* Combat = IsValid(Monster) ? Monster->GetGamePlatformCombatComponent() : nullptr;
    FApprovedWorldScope Scope;
    if (bClosing || !Combat || !Event.EventId.IsValid() || Event.EventType != EGamePlatformCombatEventType::Death ||
        Event.TargetActor != Monster || Event.TargetAvatarGeneration <= 0 ||
        Event.TargetAvatarGeneration != Combat->GetCombatAvatarGeneration() || !Combat->IsCombatDead()) return;
    if (!CaptureApprovedWorldScope(Scope) || !(Scope == OwnedWorldScope)) { CloseOwnedScope(); return; }
    if (PendingRespawnGeneration == Event.TargetAvatarGeneration && RespawnOperationId.IsValid()) return;
    GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
    PendingRespawnGeneration = Event.TargetAvatarGeneration;
    const FGuid Ticket = FGuid::NewGuid();
    RespawnOperationId = Ticket;
    const int32 ExpectedGeneration = PendingRespawnGeneration;
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer,
        FTimerDelegate::CreateWeakLambda(this, [this, Ticket, ExpectedGeneration]
        { RespawnOwnedMonster(Ticket, ExpectedGeneration); }), 10.0f, false);
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::RespawnOwnedMonster(FGuid Ticket, int32 ExpectedGeneration)
{
    if (bClosing || !Ticket.IsValid() || RespawnOperationId != Ticket || PendingRespawnGeneration != ExpectedGeneration) return;
    // 只摘本次Ticket；重生通知若同步造成后继死亡，新Timer不得被本调用尾部清理。
    RespawnOperationId.Invalidate();
    PendingRespawnGeneration = 0;
    GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
    FApprovedWorldScope Scope;
    if (!CaptureApprovedWorldScope(Scope) || !(Scope == OwnedWorldScope)) { CloseOwnedScope(); return; }
    auto* Monster = OwnedMonster.Get();
    auto* Combat = IsValid(Monster) ? Monster->GetGamePlatformCombatComponent() : nullptr;
    if (!Combat || !Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != ExpectedGeneration) return;
    FTransform Transform;
    if (!FindSafeSpawnTransform(Transform, Monster))
    {
        UE_LOG(LogTemp, Warning, TEXT("DevelopmentMonsterRespawnRejected: 10秒到期但无安全地面/胶囊候选，保持死亡。"));
        return;
    }
    const FGuid Operation = WorldOperationId;
    const bool bNeedsMove = !Monster->GetActorLocation().Equals(Transform.GetLocation()) ||
        !Monster->GetActorRotation().Equals(Transform.Rotator());
    // 无需移动时不把RootComponent的“未移动”返回值误认为重生失败。
    if (bNeedsMove && !Monster->SetActorLocationAndRotation(Transform.GetLocation(), Transform.Rotator(),
        false, nullptr, ETeleportType::TeleportPhysics)) return;
    if (bClosing || WorldOperationId != Operation || OwnedMonster != Monster ||
        !CaptureApprovedWorldScope(Scope) || !(Scope == OwnedWorldScope)) return;
    const bool bRespawned = Monster->AuthorityRespawnDevelopmentMonster(ExpectedGeneration);
    if (!bRespawned && !bClosing)
        UE_LOG(LogTemp, Warning, TEXT("DevelopmentMonsterRespawnRejected: 真实角色重生拒绝，未伪造生命恢复。"));
    // 通知可能同步退出，不能继续访问旧Actor或把后继世界记为本次重生成功。
    if (bRespawned && !bClosing && WorldOperationId == Operation && IsValid(Monster) && OwnedMonster == Monster &&
        CaptureApprovedWorldScope(Scope) && Scope == OwnedWorldScope)
    {
        const auto Life = Monster->GetLifeState();
        UE_LOG(LogTemp, Log, TEXT("DevelopmentMonsterRespawned PreviousGeneration=%d Generation=%d Health=%.0f"),
            ExpectedGeneration, Life.AvatarGeneration, Life.Health);
    }
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::HandleOwnedMonsterDestroyed(AActor* DestroyedActor)
{
    if (OwnedMonster.Get() == DestroyedActor) CloseOwnedScope();
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::CloseOwnedScope()
{
    if (bClosing) return;
    bClosing = true;
    bCreatingMonster = false;
    WorldOperationId.Invalidate();
    RespawnOperationId.Invalidate();
    PendingRespawnGeneration = 0;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RespawnTimer);
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> Monster(OwnedMonster.Get());
    OwnedMonster = nullptr;
    if (Monster.IsValid())
    {
        Monster->OnAuthoritativeDeath().Remove(DeathSubscription);
        Monster->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleOwnedMonsterDestroyed);
    }
    DeathSubscription.Reset();
    if (auto* Experience = ObservedExperience.Get()) Experience->Unsubscribe(ExperienceSubscription);
    ExperienceSubscription = {};
    ObservedExperience.Reset();
    if (auto* Lifecycle = ObservedLifecycle.Get()) Lifecycle->OnLifecycleChanged().Remove(LifecycleSubscription);
    LifecycleSubscription.Reset();
    ObservedLifecycle.Reset();
    if (Monster.IsValid() && !Monster->IsActorBeingDestroyed() && Monster->HasAuthority()) Monster->Destroy();
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
    if (&InWorld == GetWorld()) CloseOwnedScope();
    Super::OnWorldEndPlay(InWorld);
}

void UDivineBeastsDevelopmentMonsterWorldSubsystem::Deinitialize()
{
    CloseOwnedScope();
    Super::Deinitialize();
}
