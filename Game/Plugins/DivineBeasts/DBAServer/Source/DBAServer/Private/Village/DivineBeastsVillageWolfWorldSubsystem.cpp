// SSLM狼怪物专服装配：客户端只接收Actor移动/生命及AI状态，不执行生成、攻击或独立PCG决定。
#include "Village/DivineBeastsVillageWolfWorldSubsystem.h"

#include "Village/DivineBeastsVillageWolfAIController.h"
#include "Village/DivineBeastsVillageWolfSpawnPolicy.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/GamePlatformAIStateComponent.h"
<<<<<<< HEAD
#include "Components/GamePlatformCombatComponent.h" // 服务器死亡判定必须包含完整组件定义。
=======
#include "Components/GamePlatformCombatComponent.h"
>>>>>>> 2099daa15f2914ed7e018b3b28408a637afe408e
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
#include "UObject/SoftObjectPath.h" // 服务器使用蓝图软类，不引用客户端第三方狼网格。
#include "NavigationSystem.h"
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"

namespace
{
    /** 原有死亡退场与刷新延迟不变，狼群上限和位置间距统一由项目散布策略管理。 */
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
    FString WolfClassName;
    GConfig->GetString(Section, TEXT("WolfAIDefinition"), DefinitionPath, GGameIni);
    GConfig->GetString(Section, TEXT("WolfBlueprintClass"), WolfClassName, GGameIni);
    const FSoftObjectPath Definition(DefinitionPath);
    const FSoftClassPath WolfBlueprint(WolfClassName);
    // 两份服务器安全原生资源均必须存在；Fab美术仅客户端加载，绝不允许隐藏攻击怪。
    return bEnabled && bClientAssetsVerified && Definition.IsValid() && WolfBlueprint.IsValid() &&
        FPackageName::IsValidLongPackageName(Definition.GetLongPackageName()) &&
        FPackageName::DoesPackageExist(Definition.GetLongPackageName()) &&
        FPackageName::IsValidLongPackageName(WolfBlueprint.GetLongPackageName()) &&
        FPackageName::DoesPackageExist(WolfBlueprint.GetLongPackageName());
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
<<<<<<< HEAD
=======
    // 平台GetReadiness接口非const；保留服务原始接口类型，不改变本桥的只读资格检查职责。
>>>>>>> 2099daa15f2914ed7e018b3b28408a637afe408e
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
    using FPolicy = FDivineBeastsVillageWolfSpawnPolicy;
    UWorld* World = GetWorld();
    auto* Nav = IsValid(World) ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    const auto* WolfDefaults = GetDefault<ADivineBeastsVillageWolf>();
    const auto* Capsule = WolfDefaults->GetCapsuleComponent();
    if (!World || World->bIsTearingDown || !Nav || !Capsule ||
        SpawnOrdinal < 0 || SpawnOrdinal >= FPolicy::MaxAliveWolves) return false;

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

    // 不再集中在出生点26/34/42米的狭窄三圈；在45至290米范围内有限采样。
    // 优先轮换不同PlayerStart，避免多玩家出生点附近的怪群都集中于第一个出生点。
    // 真实服务器根据导航、坡度、占位和历史生成锚点裁剪；无安全点则宁可少于20只。
    for (int32 StartOffset = 0; StartOffset < Starts.Num(); ++StartOffset)
    {
        const APlayerStart* Start = Starts[(StartOffset + SpawnOrdinal) % Starts.Num()];
        for (int32 Ring = 0; Ring < FPolicy::RingCount; ++Ring)
        for (int32 Direction = 0; Direction < FPolicy::DirectionsPerRing; ++Direction)
        {
            const FVector Candidate = FPolicy::MakeCandidate(
                Start->GetActorLocation(), Ring, Direction, SpawnOrdinal);
            FNavLocation NavPoint;
            if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(130.0f, 130.0f, 550.0f)))
                continue;

<<<<<<< HEAD
            bool bSafe = true;
            for (const APlayerStart* AnyStart : Starts)
                if (!FPolicy::IsOutsidePlayerStart(NavPoint.Location, AnyStart->GetActorLocation()))
                { bSafe = false; break; }
            // 使用服务器登记的原始生成锚点，避免巡逻/追击后因Actor移走而在旧位置密集补刷。
            for (const auto& Entry : WolfSpawnAnchors)
            {
                const ADivineBeastsVillageWolf* Other = Entry.Key.Get();
                if (IsValid(Other) && !Other->IsActorBeingDestroyed() &&
                    !FPolicy::HasSpawnSeparation(NavPoint.Location, Entry.Value))
                { bSafe = false; break; }
            }
            if (!bSafe) continue;
=======
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
>>>>>>> 2099daa15f2914ed7e018b3b28408a637afe408e

            FHitResult Ground;
            if (!World->LineTraceSingleByChannel(Ground, NavPoint.Location + FVector(0,0,260),
                NavPoint.Location - FVector(0,0,650), ECC_Visibility, Query) ||
                !Ground.bBlockingHit || Ground.ImpactNormal.Z < WolfDefaults->GetCharacterMovement()->GetWalkableFloorZ())
                continue;
            const FVector Center = Ground.ImpactPoint + FVector(0,0,HalfHeight + 4);

            // 最终落点重新按真实地表XY严格核验：水平距离必须>3000厘米，刚好30米也拒绝。
            for (const APlayerStart* AnyStart : Starts)
                if (!FPolicy::IsOutsidePlayerStart(Center, AnyStart->GetActorLocation()))
                { bSafe = false; break; }
            for (const auto& Entry : WolfSpawnAnchors)
            {
                const ADivineBeastsVillageWolf* Other = Entry.Key.Get();
                if (IsValid(Other) && !Other->IsActorBeingDestroyed() &&
                    !FPolicy::HasSpawnSeparation(Center, Entry.Value))
                { bSafe = false; break; }
            }
            if (!bSafe || World->OverlapAnyTestByObjectType(Center, FQuat::Identity, Occupancy,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
                continue;

            const FVector FacingStart = Start->GetActorLocation() - Center;
            OutTransform = FTransform(FRotator(0, FacingStart.Rotation().Yaw, 0), Center);
            return true;
        }
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
    // 服务端首次加载的是不含任何狼美术的项目蓝图子类；缓存只活于本世界。
    if (!IsValid(WolfActorClass))
    {
        FString WolfClassName;
        if (!GConfig->GetString(TEXT("DivineBeasts.VillageWolves"),
            TEXT("WolfBlueprintClass"), WolfClassName, GGameIni)) return false;
        WolfActorClass = FSoftClassPath(WolfClassName).TryLoadClass<ADivineBeastsVillageWolf>();
    }
    if (!IsValid(WolfActorClass) || !WolfActorClass->IsChildOf(ADivineBeastsVillageWolf::StaticClass()))
        return false;

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    ADivineBeastsVillageWolf* Wolf =
        World->SpawnActor<ADivineBeastsVillageWolf>(WolfActorClass, SpawnAt.GetLocation(), SpawnAt.Rotator(), Spawn);
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
    // 记录真实出生锚点，而非追击后的当前位置；未成功初始化的半成品从不占用散布间距。
    WolfSpawnAnchors.Add(TWeakObjectPtr<ADivineBeastsVillageWolf>(Wolf), Wolf->GetActorLocation());
    Wolf->OnAuthoritativeDeath().AddUObject(this,&ThisClass::HandleWolfDeath);
    Wolf->OnDestroyed.AddDynamic(this,&ThisClass::HandleWolfDestroyed);
    OwnedScope=Original;
    UE_LOG(LogTemp,Log,TEXT("VillageWolfSpawned: 专服资格与Combat初始化成功；AI定义/客户端外观仍按租约异步加载，LocationCm=%s"),
        *Wolf->GetActorLocation().ToString());
    return true;
}

void UDivineBeastsVillageWolfWorldSubsystem::TryFillWolves()
{
    using FPolicy = FDivineBeastsVillageWolfSpawnPolicy;
    if (bClosing || bCreating) return;
    UWorld* World = GetWorld();
    // 死亡刷新仍遵守14秒冷却，不允许尚未完成的初始批次提前补充死狼。
    if (!World || World->bIsTearingDown || World->GetTimerManager().IsTimerActive(ReplenishTimer))
        return;
    FApprovedScope Scope;
    if (!CaptureApprovedScope(Scope)) return;
    if (OwnedScope.WorldGeneration.IsValid() && !(OwnedScope == Scope))
    { CloseScope(); return; }
    bCreating = true;

    // 实际存活实体与尚未销毁的尸体分开计数；尸体原始出生锚点一直保留到销毁时。
    OwnedWolves.RemoveAll([](const auto& Wolf) {
        return !IsValid(Wolf) || Wolf->IsActorBeingDestroyed();
    });
    for (auto It = WolfSpawnAnchors.CreateIterator(); It; ++It)
    {
        ADivineBeastsVillageWolf* Owner = It.Key().Get();
        if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || !OwnedWolves.Contains(Owner))
            It.RemoveCurrent();
    }
    int32 Alive = 0;
    for (const auto& Wolf : OwnedWolves)
        if (IsValid(Wolf) && !Wolf->IsActorBeingDestroyed() && Wolf->IsWolfAlive())
            ++Alive;

    // 四只一批、0.2秒间隔，分散NavMesh投射/占位查询的同帧开销；严格20只上限。
    int32 SpawnedThisBatch = 0;
    bool bBlocked = false;
    for (int32 Ordinal = Alive; Ordinal < FPolicy::MaxAliveWolves &&
        SpawnedThisBatch < FPolicy::MaxWolvesPerBatch && !bClosing; ++Ordinal)
    {
        FTransform SpawnAt;
        if (!FindSpawnTransform(Ordinal, SpawnAt))
        {
            UE_LOG(LogTemp, Warning, TEXT("VillageWolfScatterLimited: 活体%d/上限%d；没有满足>30米出生锚点间距、NavMesh、地形或22米安全区的有效位置。"),
                Alive, FPolicy::MaxAliveWolves);
            bBlocked = true;
            break;
        }
        if (!SpawnOneWolf(SpawnAt))
        {
            UE_LOG(LogTemp, Warning, TEXT("VillageWolfScatterBlocked: 生成或初始化失败，已有活体%d只，禁止生成不可见敌人。"), Alive);
            bBlocked = true;
            break;
        }
        ++Alive;
        ++SpawnedThisBatch;
    }
    bCreating = false;

    if (!bBlocked && SpawnedThisBatch > 0 && Alive < FPolicy::MaxAliveWolves &&
        !bClosing && !World->bIsTearingDown)
    {
        // 只保留本世界单个补充Timer；世界重建后的迟到委托无法操作下一代世界。
        const FGuid ExpectedOperation = WorldOperation;
        World->GetTimerManager().SetTimer(SpawnBatchTimer,
            FTimerDelegate::CreateWeakLambda(this, [this, ExpectedOperation]
            {
                if (!bClosing && ExpectedOperation == WorldOperation)
                    TryFillWolves();
            }), FPolicy::SpawnBatchIntervalSeconds, false);
    }
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleWolfDeath(const FGamePlatformCombatEvent& Event)
{
    auto* Wolf=Cast<ADivineBeastsVillageWolf>(Event.TargetActor.Get());
    if (bClosing || !IsValid(Wolf) || !OwnedWolves.Contains(Wolf) ||
        Event.EventType!=EGamePlatformCombatEventType::Death || !Wolf->GetGamePlatformCombatComponent()->IsCombatDead()) return;
    Wolf->SetLifeSpan(WolfCorpseLifetimeSeconds);
    // 停止尚未执行的初始批次，避免14秒重生冷却期间提前补满。
    GetWorld()->GetTimerManager().ClearTimer(SpawnBatchTimer);
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
    WolfSpawnAnchors.Remove(TWeakObjectPtr<ADivineBeastsVillageWolf>(Wolf));
    GetWorld()->GetTimerManager().ClearTimer(SpawnBatchTimer);
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
    if (auto* World=GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReplenishTimer);
        World->GetTimerManager().ClearTimer(SpawnBatchTimer);
    }
    WolfActorClass = nullptr;
    if (auto* Exp=ObservedExperience.Get()) Exp->Unsubscribe(ExperienceSubscription);
    ExperienceSubscription={};
    ObservedExperience.Reset();
    if (auto* Life=ObservedLifecycle.Get()) Life->OnLifecycleChanged().Remove(LifecycleSubscription);
    LifecycleSubscription.Reset();
    ObservedLifecycle.Reset();
    TArray<TObjectPtr<ADivineBeastsVillageWolf>> ToDestroy = MoveTemp(OwnedWolves);
    OwnedWolves.Reset();
<<<<<<< HEAD
    // 先释放本世界的锚点账本，再销毁自有Actor，EndPlay期间不会错误重占位置。
    WolfSpawnAnchors.Reset();
    for (const auto& WolfPointer : ToDestroy)
    {
        ADivineBeastsVillageWolf* Wolf = WolfPointer.Get();
=======
    for (const auto& WolfReference:ToDestroy)
    {
        auto* Wolf=WolfReference.Get();
>>>>>>> 2099daa15f2914ed7e018b3b28408a637afe408e
        if (!IsValid(Wolf)) continue;
        Wolf->OnAuthoritativeDeath().RemoveAll(this);
        Wolf->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWolfDestroyed);
        AController* Controller = Wolf->GetController();
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
