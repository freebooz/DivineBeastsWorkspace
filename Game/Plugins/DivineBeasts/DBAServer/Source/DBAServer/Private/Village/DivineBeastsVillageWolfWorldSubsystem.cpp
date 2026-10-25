// SSLM狼怪物专服装配：客户端只接收Actor移动/生命及AI状态，不执行生成、攻击或独立PCG决定。
#include "Village/DivineBeastsVillageWolfWorldSubsystem.h"

#include "Village/DivineBeastsVillageWolfAIController.h"
#include "Village/DivineBeastsVillageWolfSpawnPolicy.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Characters/DivineBeastsVillageNPC.h"
#include "Characters/DivineBeastsVillageVampire.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformCombatComponent.h" // 服务器死亡判定必须包含完整组件定义。
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
#include "NavigationData.h"
#include "Actors/GamePlatformPCGActors.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "HAL/PlatformTime.h"
#include "Server/DivineBeastsServerBootstrapSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Types/GamePlatformCombatEvent.h"

namespace
{
    /** 原有死亡退场与刷新延迟不变，狼群上限和位置间距统一由项目散布策略管理。 */
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
    ObserveNavigation();
    ObserveExperience();
    TryFillWolves();
}

bool UDivineBeastsVillageWolfWorldSubsystem::HasVerifiedAnimalAssets() const
{
    if (!GConfig) return false;
    bool bEnabled = false, bClientAssetsVerified = false, bVampireClientAssetsVerified = false;
    const TCHAR* Section = TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section, TEXT("bEnableVillageWolves"), bEnabled, GGameIni);
    GConfig->GetBool(Section, TEXT("bClientVisualsVerified"), bClientAssetsVerified, GGameIni);
    GConfig->GetBool(Section, TEXT("bVampireClientVisualsVerified"), bVampireClientAssetsVerified, GGameIni);
    FString DefinitionPath;
    FString WolfClassName;
    GConfig->GetString(Section, TEXT("WolfAIDefinition"), DefinitionPath, GGameIni);
    GConfig->GetString(Section, TEXT("WolfBlueprintClass"), WolfClassName, GGameIni);
    const FSoftObjectPath Definition(DefinitionPath);
    const FSoftClassPath WolfBlueprint(WolfClassName);
    // 两份服务器安全资源都必须存在；只检查权威包，不访问客户端模型与动画。
    return bEnabled && bClientAssetsVerified && bVampireClientAssetsVerified && Definition.IsValid() && WolfBlueprint.IsValid() &&
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
    ObserveNavigation();
    ObserveExperience();
    TryFillWolves();
}

void UDivineBeastsVillageWolfWorldSubsystem::ObserveNavigation()
{
    if(bClosing||!WorldOperation.IsValid()||ObservedNavigation.IsValid())return;
    auto* Navigation=GetWorld()?UNavigationSystemV1::GetCurrent(GetWorld()):nullptr;
    if(!Navigation)return;
    ObservedNavigation=Navigation;
    Navigation->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(this,&ThisClass::HandleNavigationFinished);
}

void UDivineBeastsVillageWolfWorldSubsystem::HandleNavigationFinished(ANavigationData* NavigationData)
{
    if(bClosing||!IsValid(NavigationData)||NavigationData->GetWorld()!=GetWorld())return;
    CachedSpawnGrounds.Reset();
    bSpawnCandidatesBuilt = false;
    // 每次有限补齐仍重新验证角色、体验、代次、NavMesh与安全区；重复通知不会超过四只。
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

bool UDivineBeastsVillageWolfWorldSubsystem::BuildSpawnCandidates() const
{
    using FPolicy = FDivineBeastsVillageWolfSpawnPolicy;
    if (bSpawnCandidatesBuilt) return !CachedSpawnGrounds.IsEmpty();
    UWorld* World = GetWorld();
    auto* Nav = IsValid(World) ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    if (!World || World->bIsTearingDown || !Nav) return false;
    const ANavigationData* NavData = Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
    if (!NavData) return false;
    TArray<FBox> NavBounds;
    Nav->GetNavigationBoundsForNavData(*NavData, NavBounds);
    FBox Bounds(ForceInit);
    for (const FBox& Box : NavBounds) Bounds += Box;
    if (!Bounds.IsValid) return false;
    const int32 NX = FMath::FloorToInt(Bounds.GetSize().X / FPolicy::CandidateGridStepCm);
    const int32 NY = FMath::FloorToInt(Bounds.GetSize().Y / FPolicy::CandidateGridStepCm);
    if (NX <= 0 || NY <= 0 || static_cast<int64>(NX) * NY > FPolicy::MaxCandidateCells)
    {
        UE_LOG(LogTemp, Error, TEXT("VillageMonsterCandidatesRefused: 导航范围超出有界网格预算。"));
        return false;
    }

    // 只读平台WorldDirector明确登记的数值几何，不运行装饰PCG，不复制空间算法。
    TArray<FGamePlatformPCGSpatialMask> Masks;
    bool bHasDirector = false;
    for (TActorIterator<AGamePlatformPCGWorldDirector> It(World); It; ++It)
    {
        TArray<FGamePlatformPCGSpatialMask> Local;
        FString Error;
        if (!It->CollectSpatialMasks(Local, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("VillageMonsterCandidatesRefused: PCG排除数据无效 %s"), *Error);
            return false;
        }
        bHasDirector = true;
        Masks.Append(MoveTemp(Local));
    }
    if (!bHasDirector) return false; // 缺少已审查排除合同，不能忽略水域/道路继续刷怪。
    for (auto& Mask : Masks) Mask.HalfWidthCm += 100.f;
    TArray<FGamePlatformPCGPreparedSpatialMask> Prepared;
    if (!FGamePlatformPCGSpatialRules::PrepareMasks(Masks, Prepared)) return false;
    const auto IsExcluded = [&Prepared](const FVector& Point)
    {
        float Strength = 0.f;
        FGuid Source;
        return !FGamePlatformPCGSpatialRules::EvaluatePrepared(FVector2D(Point), 0, .5f,
            Prepared, Strength, Source) || Strength >= .5f;
    };
    const double Started = FPlatformTime::Seconds();
    const float FloorZ = GetDefault<ADivineBeastsVillageWolf>()->GetCharacterMovement()->GetWalkableFloorZ();
    FCollisionQueryParams Query(TEXT("VillageMonsterCandidateGround"), false);
    FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
    TArray<FVector> Starts;
    for (TActorIterator<APlayerStart> It(World); It; ++It) Starts.Add(It->GetActorLocation());
    if (Starts.IsEmpty()) return false;
    for (int32 Y = 0; Y < NY; ++Y)
    for (int32 X = 0; X < NX; ++X)
    {
        FVector Point(Bounds.Min.X + (X + .5f) * FPolicy::CandidateGridStepCm,
            Bounds.Min.Y + (Y + .5f) * FPolicy::CandidateGridStepCm, Bounds.Max.Z + 500.f);
        if (IsExcluded(Point)) continue;
        FHitResult Ground;
        if (!World->LineTraceSingleByObjectType(Ground, Point,
            FVector(Point.X, Point.Y, Bounds.Min.Z - 500.f), GroundObjects, Query) ||
            Ground.ImpactNormal.Z < FloorZ) continue;
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Ground.ImpactPoint, Projected, FVector(130.f,130.f,300.f)) ||
            FMath::Abs(Projected.Location.Z - Ground.ImpactPoint.Z) > 150.f || IsExcluded(Projected.Location)) continue;
        Point = Projected.Location;
        bool bSafe = true;
        for (const FVector& Start : Starts)
            if (!FPolicy::IsOutsidePlayerStart(Point, Start)) { bSafe = false; break; }
        for (const FVector& Existing : CachedSpawnGrounds)
            if (!FPolicy::HasSpawnSeparation(Point, Existing)) { bSafe = false; break; }
        if (bSafe) CachedSpawnGrounds.Add(Point);
    }
    FRandomStream Random(FPolicy::ScatterSeed);
    for (int32 I = CachedSpawnGrounds.Num() - 1; I > 0; --I)
        CachedSpawnGrounds.Swap(I, Random.RandRange(0, I));
    bSpawnCandidatesBuilt = true;
    UE_LOG(LogTemp, Display,
        TEXT("VillageMonsterCandidates ready=%d target=%d spacingCm=%.0f masks=%d cells=%d buildMs=%.2f seed=%d"),
        CachedSpawnGrounds.Num(), FPolicy::MaxAliveWolves, FPolicy::MinimumSpawnSeparationCm,
        Masks.Num(), NX * NY, (FPlatformTime::Seconds() - Started) * 1000., FPolicy::ScatterSeed);
    return !CachedSpawnGrounds.IsEmpty();
}

bool UDivineBeastsVillageWolfWorldSubsystem::FindSpawnTransform(
    int32 SpawnOrdinal, bool bVampire, FTransform& OutTransform) const
{
    using FPolicy = FDivineBeastsVillageWolfSpawnPolicy;
    UWorld* World = GetWorld();
    auto* Nav = IsValid(World) ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    // 人型怪物胶囊高于野狼；落点和占位都必须按实际要生成的类别计算。
    const ADivineBeastsVillageWolf* CreatureDefaults = bVampire
        ? static_cast<const ADivineBeastsVillageWolf*>(GetDefault<ADivineBeastsVillageVampire>())
        : GetDefault<ADivineBeastsVillageWolf>();
    const auto* Capsule = CreatureDefaults->GetCapsuleComponent();
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

    if (!BuildSpawnCandidates()) return false;
    // 缓存已按固定种子打乱；每次仍复核权威导航、胶囊占位及原始生成锚点。
    const APlayerStart* Start = Starts[SpawnOrdinal % Starts.Num()];
    for (int32 Offset = 0; Offset < CachedSpawnGrounds.Num(); ++Offset)
    {
            const FVector Candidate = CachedSpawnGrounds[(Offset + SpawnOrdinal) % CachedSpawnGrounds.Num()];
            FNavLocation NavPoint;
            if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(130.0f, 130.0f, 550.0f)))
                continue;

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

            FHitResult Ground;
            if (!World->LineTraceSingleByChannel(Ground, NavPoint.Location + FVector(0,0,260),
                NavPoint.Location - FVector(0,0,650), ECC_Visibility, Query) ||
                !Ground.bBlockingHit || Ground.ImpactNormal.Z < CreatureDefaults->GetCharacterMovement()->GetWalkableFloorZ())
                continue;
            const FVector Center = Ground.ImpactPoint + FVector(0,0,
                FPolicy::SpawnHeightOnSlope(HalfHeight, Radius, Ground.ImpactNormal.Z));

            // 最终落点按地表XY复核：水平距离必须>2400厘米。
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
    return false;
}

bool UDivineBeastsVillageWolfWorldSubsystem::FindNPCSpawnTransform(
    int32 SpawnOrdinal, FTransform& OutTransform) const
{
    UWorld* World = GetWorld();
    auto* Nav = IsValid(World) ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    const auto* Defaults = GetDefault<ADivineBeastsVillageNPC>();
    const auto* Capsule = Defaults->GetCapsuleComponent();
    if (!World || World->bIsTearingDown || !Nav || !Capsule ||
        SpawnOrdinal < 0 || SpawnOrdinal >= 3) return false;

    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A, const APlayerStart& B)
        { return A.GetPathName() < B.GetPathName(); });
    if (Starts.IsEmpty()) return false;

    FCollisionQueryParams Query(TEXT("VillageNPCSafeSpawn"), false);
    FCollisionObjectQueryParams Occupancy;
    Occupancy.AddObjectTypesToQuery(ECC_WorldStatic);
    Occupancy.AddObjectTypesToQuery(ECC_WorldDynamic);
    Occupancy.AddObjectTypesToQuery(ECC_Pawn);
    Occupancy.AddObjectTypesToQuery(ECC_PhysicsBody);
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    // NPC处于怪物22米禁刷区内部；角度和半径分散，近处便于玩家发现。
    constexpr float DistancesCm[] = {900.0f, 1400.0f, 1900.0f, 2150.0f};
    for (int32 StartOffset = 0; StartOffset < Starts.Num(); ++StartOffset)
    {
        const APlayerStart* Start = Starts[(SpawnOrdinal + StartOffset) % Starts.Num()];
        for (float DistanceCm : DistancesCm)
        for (int32 Direction = 0; Direction < 24; ++Direction)
        {
            const int32 Index = (Direction * 7 + SpawnOrdinal * 8) % 24;
            const float Angle = 2.0f * PI * static_cast<float>(Index) / 24.0f;
            const FVector Candidate = Start->GetActorLocation() +
                FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * DistanceCm;
            FNavLocation NavPoint;
            if (!Nav->ProjectPointToNavigation(Candidate, NavPoint, FVector(130, 130, 550)))
                continue;
            FHitResult Ground;
            if (!World->LineTraceSingleByChannel(Ground,
                    NavPoint.Location + FVector(0, 0, 260),
                    NavPoint.Location - FVector(0, 0, 650), ECC_Visibility, Query) ||
                !Ground.bBlockingHit ||
                Ground.ImpactNormal.Z < Defaults->GetCharacterMovement()->GetWalkableFloorZ())
                continue;
            const FVector Center = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 4);
            bool bSeparated = true;
            for (const auto& NPC : OwnedNPCs)
                if (IsValid(NPC) && !NPC->IsActorBeingDestroyed() &&
                    FVector::DistSquared2D(Center, NPC->GetActorLocation()) < FMath::Square(450.0f))
                { bSeparated = false; break; }
            if (!bSeparated || World->OverlapAnyTestByObjectType(Center, FQuat::Identity,
                    Occupancy, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
                continue;
            const FVector FacingStart = Start->GetActorLocation() - Center;
            OutTransform = FTransform(FRotator(0, FacingStart.Rotation().Yaw, 0), Center);
            return true;
        }
    }
    return false;
}

void UDivineBeastsVillageWolfWorldSubsystem::TryFillNPCs(const FApprovedScope& Scope)
{
    UWorld* World = GetWorld();
    if (!World || bClosing || World->bIsTearingDown) return;
    OwnedNPCs.RemoveAll([](const auto& NPC)
        { return !IsValid(NPC) || NPC->IsActorBeingDestroyed(); });
    for (int32 Ordinal = OwnedNPCs.Num(); Ordinal < 3; ++Ordinal)
    {
        FTransform SpawnAt;
        if (!FindNPCSpawnTransform(Ordinal, SpawnAt))
        {
            UE_LOG(LogTemp, Warning, TEXT("VillageNPCScatterLimited: 已生成%d/3；等待有效导航与地面落点。"),
                OwnedNPCs.Num());
            break;
        }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
        auto* NPC = World->SpawnActor<ADivineBeastsVillageNPC>(
            SpawnAt.GetLocation(), SpawnAt.Rotator(), Spawn);
        FApprovedScope Current;
        if (!IsValid(NPC) || !CaptureApprovedScope(Current) || !(Current == Scope))
        {
            if (IsValid(NPC)) NPC->Destroy();
            break;
        }
        OwnedNPCs.Add(NPC);
        OwnedScope = Scope;
        UE_LOG(LogTemp, Log, TEXT("VillageNPCSpawned: Arissa NPC %d/3，LocationCm=%s"),
            OwnedNPCs.Num(), *NPC->GetActorLocation().ToString());
    }
}

bool UDivineBeastsVillageWolfWorldSubsystem::SpawnOneWolf(const FTransform& SpawnAt, bool bVampire)
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
    UClass* CreatureClass = bVampire ? ADivineBeastsVillageVampire::StaticClass() : WolfActorClass.Get();
    ADivineBeastsVillageWolf* Wolf =
        World->SpawnActor<ADivineBeastsVillageWolf>(CreatureClass, SpawnAt.GetLocation(), SpawnAt.Rotator(), Spawn);
    if (!IsValid(Wolf)) return false;
    Wolf->GetWolfAIState()->SetDefinitionAsset(
        TSoftObjectPtr<UGamePlatformAIDefinition>(SoftDefinition));
    auto* Controller=World->SpawnActor<ADivineBeastsVillageWolfAIController>(
        SpawnAt.GetLocation(),SpawnAt.Rotator());
    if (IsValid(Controller)) Controller->Possess(Wolf);
    FApprovedScope Current;
    const bool bControllerBound=IsValid(Controller)&&IsValid(Wolf)&&Wolf->GetController()==Controller;
    const bool bCombatReady=bControllerBound&&Wolf->AuthorityInitializeWolf(100.0f);
    const bool bScopeCurrent=bCombatReady&&CaptureApprovedScope(Current)&&(Current==Original);
    if (!bControllerBound || !bCombatReady || !bScopeCurrent)
    {
        UE_LOG(LogTemp,Warning,TEXT("VillageWolfSpawnRejected: ControllerBound=%d CombatReady=%d ScopeCurrent=%d"),
            bControllerBound,bCombatReady,bScopeCurrent);
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
    TryFillNPCs(Scope);
    if (bClosing) return;
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

    // 四只一批、0.2秒间隔，分散AI/导航启动峰值；两类怪物合计200只上限。
    int32 SpawnedThisBatch = 0;
    bool bBlocked = false;
    for (int32 Ordinal = Alive; Ordinal < FPolicy::MaxAliveWolves &&
        SpawnedThisBatch < FPolicy::MaxWolvesPerBatch && !bClosing; ++Ordinal)
    {
        FTransform SpawnAt;
        const bool bVampire = Ordinal % 2 == 1;
        if (!FindSpawnTransform(Ordinal, bVampire, SpawnAt))
        {
            UE_LOG(LogTemp, Warning, TEXT("VillageWolfScatterLimited: 活体%d/上限%d；没有满足>24米锚点间距、NavMesh、地形或22米安全区的有效位置。"),
                Alive, FPolicy::MaxAliveWolves);
            bBlocked = true;
            break;
        }
        if (!SpawnOneWolf(SpawnAt, bVampire))
        {
            UE_LOG(LogTemp, Warning, TEXT("VillageWolfScatterBlocked: 生成或初始化失败，已有活体%d只，禁止生成不可见敌人。"), Alive);
            bBlocked = true;
            break;
        }
        ++Alive;
        ++SpawnedThisBatch;
    }
    bCreating = false;
    UE_LOG(LogTemp, Display, TEXT("VillageMonsterPopulation alive=%d target=%d spawnedThisBatch=%d candidates=%d"),
        Alive, FPolicy::MaxAliveWolves, SpawnedThisBatch, CachedSpawnGrounds.Num());

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
    Wolf->SetLifeSpan(ADivineBeastsVillageWolf::DeathCorpseLifetimeSeconds);
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
    if(auto* Navigation=ObservedNavigation.Get())
        Navigation->OnNavigationGenerationFinishedDelegate.RemoveDynamic(this,&ThisClass::HandleNavigationFinished);
    ObservedNavigation.Reset();
    WolfActorClass = nullptr;
    if (auto* Exp=ObservedExperience.Get()) Exp->Unsubscribe(ExperienceSubscription);
    ExperienceSubscription={};
    ObservedExperience.Reset();
    if (auto* Life=ObservedLifecycle.Get()) Life->OnLifecycleChanged().Remove(LifecycleSubscription);
    LifecycleSubscription.Reset();
    ObservedLifecycle.Reset();
    TArray<TObjectPtr<ADivineBeastsVillageWolf>> ToDestroy = MoveTemp(OwnedWolves);
    OwnedWolves.Reset();
    TArray<TObjectPtr<ADivineBeastsVillageNPC>> NPCsToDestroy = MoveTemp(OwnedNPCs);
    OwnedNPCs.Reset();
    // 先释放本世界的锚点账本，再销毁自有Actor，EndPlay期间不会错误重占位置。
    WolfSpawnAnchors.Reset();
    CachedSpawnGrounds.Reset();
    bSpawnCandidatesBuilt = false;
    for (const auto& WolfPointer : ToDestroy)
    {
        ADivineBeastsVillageWolf* Wolf = WolfPointer.Get();
        if (!IsValid(Wolf)) continue;
        Wolf->OnAuthoritativeDeath().RemoveAll(this);
        Wolf->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWolfDestroyed);
        AController* Controller = Wolf->GetController();
        if (!Wolf->IsActorBeingDestroyed() && Wolf->HasAuthority()) Wolf->Destroy();
        if (IsValid(Controller) && !Controller->IsActorBeingDestroyed()) Controller->Destroy();
    }
    for (const auto& NPCPointer : NPCsToDestroy)
        if (auto* NPC = NPCPointer.Get(); IsValid(NPC) && !NPC->IsActorBeingDestroyed() && NPC->HasAuthority())
            NPC->Destroy();
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
