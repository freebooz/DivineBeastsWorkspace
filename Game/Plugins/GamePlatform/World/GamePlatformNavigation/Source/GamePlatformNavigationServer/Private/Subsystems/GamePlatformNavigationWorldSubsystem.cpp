#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"

#include "Data/GamePlatformNavigationAgentProfile.h"
#include "Filters/GamePlatformNavigationQueryFilters.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/ThreadSafeCounter.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"
#include "Settings/GamePlatformNavigationSettings.h"

namespace
{
FThreadSafeCounter GGamePlatformNavigationWorldGeneration;

FName DefaultAgentProfileId()
{
    static const FName Id(TEXT("Navigation.Agent.Default"));
    return Id;
}

FName DefaultFilterId()
{
    static const FName Id(TEXT("Navigation.Filter.Default"));
    return Id;
}

bool IsFiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN();
}
}

void UGamePlatformNavigationWorldSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    WorldGeneration =
        FMath::Max(
            1,
            GGamePlatformNavigationWorldGeneration.Increment());

    Filters.Add(
        DefaultFilterId(),
        UGamePlatformNavigationQueryFilter_Default::StaticClass());
}

void UGamePlatformNavigationWorldSubsystem::Deinitialize()
{
    TArray<FGuid> RequestIds;
    AsyncRequests.GetKeys(RequestIds);

    for (const FGuid& RequestId : RequestIds)
    {
        FGamePlatformNavigationRequestHandle Handle;
        Handle.RequestId = RequestId;
        Handle.WorldGeneration = WorldGeneration;
        CancelRequest(Handle);
    }

    TArray<TWeakObjectPtr<AActor>> Invokers =
        RegisteredInvokers.Array();

    for (const TWeakObjectPtr<AActor>& InvokerPtr : Invokers)
    {
        if (AActor* Invoker = InvokerPtr.Get())
        {
            UnregisterInvoker(*Invoker);
        }
    }

    AgentProfiles.Reset();
    Filters.Reset();
    ++WorldGeneration;

    Super::Deinitialize();
}

FGamePlatformNavigationProjectionResult
UGamePlatformNavigationWorldSubsystem::ProjectPoint(
    const FVector& Position,
    const FVector& Extent,
    FName AgentProfileId,
    FName FilterId)
{
    FGamePlatformNavigationProjectionResult Result;

    const UGamePlatformNavigationSettings* Settings =
        GetDefault<UGamePlatformNavigationSettings>();

    if (!GetWorld())
    {
        Result.Error = EGamePlatformNavigationError::InvalidWorld;
        return Result;
    }

    if (!IsFiniteVector(Position) ||
        !IsFiniteVector(Extent) ||
        Extent.X < 0.0 ||
        Extent.Y < 0.0 ||
        Extent.Z < 0.0 ||
        Extent.GetMax() > Settings->MaxProjectionExtent)
    {
        Result.Error = EGamePlatformNavigationError::ProjectionFailed;
        return Result;
    }

    FNavAgentProperties Agent;
    ANavigationData* NavData = nullptr;
    FSharedConstNavQueryFilter QueryFilter;
    EGamePlatformNavigationError Error =
        EGamePlatformNavigationError::None;

    if (!ResolveQueryContext(
            AgentProfileId,
            FilterId,
            Position,
            Agent,
            NavData,
            QueryFilter,
            Error))
    {
        Result.Error = Error;
        return Result;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        Result.Error =
            EGamePlatformNavigationError::NavigationUnavailable;
        return Result;
    }

    FNavLocation Projected;
    Result.bSuccess =
        Navigation->ProjectPointToNavigation(
            Position,
            Projected,
            Extent,
            NavData,
            QueryFilter);

    Result.ProjectedLocation =
        Result.bSuccess
            ? Projected.Location
            : FVector::ZeroVector;

    Result.Error =
        Result.bSuccess
            ? EGamePlatformNavigationError::None
            : EGamePlatformNavigationError::ProjectionFailed;

    return Result;
}

bool UGamePlatformNavigationWorldSubsystem::TestPath(
    const FGamePlatformNavigationRequest& Request,
    EGamePlatformNavigationError& OutError)
{
    ++SyncQueryCount;
    OutError = EGamePlatformNavigationError::None;

    FGuid EffectiveRequestId =
        Request.RequestId.IsValid()
            ? Request.RequestId
            : FGuid::NewGuid();

    FNavAgentProperties Agent;
    FPathFindingQuery Query;
    FVector ResolvedStart;
    FVector ResolvedGoal;

    if (!BuildPathQuery(
            Request,
            EffectiveRequestId,
            Agent,
            Query,
            ResolvedStart,
            ResolvedGoal,
            OutError))
    {
        return false;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        OutError =
            EGamePlatformNavigationError::NavigationUnavailable;
        return false;
    }

    const bool bHasPath =
        Navigation->TestPathSync(
            Query,
            EPathFindingMode::Regular,
            nullptr);

    if (!bHasPath)
    {
        OutError = EGamePlatformNavigationError::PathNotFound;
    }

    return bHasPath;
}

FGamePlatformNavigationPathResult
UGamePlatformNavigationWorldSubsystem::FindPath(
    const FGamePlatformNavigationRequest& Request)
{
    ++SyncQueryCount;

    const FGuid EffectiveRequestId =
        Request.RequestId.IsValid()
            ? Request.RequestId
            : FGuid::NewGuid();

    FNavAgentProperties Agent;
    FPathFindingQuery Query;
    FVector ResolvedStart;
    FVector ResolvedGoal;
    EGamePlatformNavigationError Error =
        EGamePlatformNavigationError::None;

    if (!BuildPathQuery(
            Request,
            EffectiveRequestId,
            Agent,
            Query,
            ResolvedStart,
            ResolvedGoal,
            Error))
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId = EffectiveRequestId;
        Result.WorldGeneration = WorldGeneration;
        Result.Status =
            EGamePlatformNavigationPathStatus::Failed;
        Result.Error = Error;
        return Result;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId = EffectiveRequestId;
        Result.WorldGeneration = WorldGeneration;
        Result.Status =
            EGamePlatformNavigationPathStatus::Failed;
        Result.Error =
            EGamePlatformNavigationError::NavigationUnavailable;
        return Result;
    }

    const FPathFindingResult EngineResult =
        Navigation->FindPathSync(
            Agent,
            Query,
            EPathFindingMode::Regular);

    return BuildPathResult(
        Request,
        EffectiveRequestId,
        WorldGeneration,
        EngineResult.Result,
        EngineResult.Path,
        ResolvedStart,
        ResolvedGoal);
}

FGamePlatformNavigationRequestHandle
UGamePlatformNavigationWorldSubsystem::FindPathAsync(
    const FGamePlatformNavigationRequest& Request,
    FGamePlatformNavigationQueryCompleted Completion)
{
    FGamePlatformNavigationRequestHandle Handle;

    const UGamePlatformNavigationSettings* Settings =
        GetDefault<UGamePlatformNavigationSettings>();

    if (AsyncRequests.Num() >=
        FMath::Max(1, Settings->MaxOutstandingAsyncRequests))
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId =
            Request.RequestId.IsValid()
                ? Request.RequestId
                : FGuid::NewGuid();
        Result.WorldGeneration = WorldGeneration;
        Result.Status = EGamePlatformNavigationPathStatus::Failed;
        Result.Error = EGamePlatformNavigationError::Unsupported;
        Completion.ExecuteIfBound(Result);
        return Handle;
    }

    const FGuid EffectiveRequestId =
        Request.RequestId.IsValid()
            ? Request.RequestId
            : FGuid::NewGuid();

    FNavAgentProperties Agent;
    FPathFindingQuery Query;
    FVector ResolvedStart;
    FVector ResolvedGoal;
    EGamePlatformNavigationError Error =
        EGamePlatformNavigationError::None;

    if (!BuildPathQuery(
            Request,
            EffectiveRequestId,
            Agent,
            Query,
            ResolvedStart,
            ResolvedGoal,
            Error))
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId = EffectiveRequestId;
        Result.WorldGeneration = WorldGeneration;
        Result.Status = EGamePlatformNavigationPathStatus::Failed;
        Result.Error = Error;
        Completion.ExecuteIfBound(Result);
        return Handle;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId = EffectiveRequestId;
        Result.WorldGeneration = WorldGeneration;
        Result.Status = EGamePlatformNavigationPathStatus::Failed;
        Result.Error =
            EGamePlatformNavigationError::NavigationUnavailable;
        Completion.ExecuteIfBound(Result);
        return Handle;
    }

    const int32 ExpectedWorldGeneration = WorldGeneration;

    const FNavPathQueryDelegate Delegate =
        FNavPathQueryDelegate::CreateUObject(
            this,
            &UGamePlatformNavigationWorldSubsystem::
                HandleAsyncPathCompleted,
            EffectiveRequestId,
            ExpectedWorldGeneration,
            ResolvedStart,
            ResolvedGoal);

    const uint32 EngineQueryId =
        Navigation->FindPathAsync(
            Agent,
            Query,
            Delegate,
            EPathFindingMode::Regular);

    if (EngineQueryId == 0)
    {
        FGamePlatformNavigationPathResult Result;
        Result.RequestId = EffectiveRequestId;
        Result.WorldGeneration = WorldGeneration;
        Result.Status = EGamePlatformNavigationPathStatus::Failed;
        Result.Error = EGamePlatformNavigationError::PathNotFound;
        Completion.ExecuteIfBound(Result);
        return Handle;
    }

    ++AsyncQueryCount;

    FAsyncRequestRecord& Record =
        AsyncRequests.Add(EffectiveRequestId);

    Record.EngineQueryId = EngineQueryId;
    Record.WorldGeneration = ExpectedWorldGeneration;
    Record.Owner = Request.OwnerScope;
    Record.bHadOwner = IsValid(Request.OwnerScope);
    Record.Request = Request;
    Record.Request.RequestId = EffectiveRequestId;
    Record.Request.WorldGeneration = ExpectedWorldGeneration;
    Record.Completion = MoveTemp(Completion);

    const float TimeoutSeconds =
        Request.TimeoutSeconds > 0.0f
            ? Request.TimeoutSeconds
            : Settings->DefaultRequestTimeoutSeconds;

    if (TimeoutSeconds > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            Record.TimeoutHandle,
            FTimerDelegate::CreateUObject(
                this,
                &UGamePlatformNavigationWorldSubsystem::
                    HandleAsyncTimeout,
                EffectiveRequestId,
                ExpectedWorldGeneration),
            TimeoutSeconds,
            false);
    }

    Handle.RequestId = EffectiveRequestId;
    Handle.WorldGeneration = ExpectedWorldGeneration;
    return Handle;
}

bool UGamePlatformNavigationWorldSubsystem::CancelRequest(
    const FGamePlatformNavigationRequestHandle& Handle)
{
    if (!Handle.IsValid() ||
        Handle.WorldGeneration != WorldGeneration)
    {
        return false;
    }

    FAsyncRequestRecord* Record =
        AsyncRequests.Find(Handle.RequestId);

    if (!Record)
    {
        return false;
    }

    FAsyncRequestRecord Local = MoveTemp(*Record);
    AsyncRequests.Remove(Handle.RequestId);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(Local.TimeoutHandle);
    }

    if (UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        if (Local.EngineQueryId != 0)
        {
            Navigation->AbortAsyncFindPathRequest(
                Local.EngineQueryId);
        }
    }

    ++CancelledRequestCount;

    FGamePlatformNavigationPathResult Result;
    Result.RequestId = Handle.RequestId;
    Result.WorldGeneration = Handle.WorldGeneration;
    Result.Status = EGamePlatformNavigationPathStatus::Cancelled;
    Result.Error = EGamePlatformNavigationError::RequestCancelled;

    Local.Completion.ExecuteIfBound(Result);
    return true;
}

bool UGamePlatformNavigationWorldSubsystem::FindRandomReachablePoint(
    const FVector& Origin,
    float Radius,
    FName AgentProfileId,
    FName FilterId,
    FVector& OutLocation,
    EGamePlatformNavigationError& OutError)
{
    OutLocation = FVector::ZeroVector;
    OutError = EGamePlatformNavigationError::None;

    if (!FMath::IsFinite(Radius) ||
        Radius <= 0.0f)
    {
        OutError = EGamePlatformNavigationError::InvalidGoal;
        return false;
    }

    FNavAgentProperties Agent;
    ANavigationData* NavData = nullptr;
    FSharedConstNavQueryFilter QueryFilter;

    if (!ResolveQueryContext(
            AgentProfileId,
            FilterId,
            Origin,
            Agent,
            NavData,
            QueryFilter,
            OutError))
    {
        return false;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        OutError =
            EGamePlatformNavigationError::NavigationUnavailable;
        return false;
    }

    FNavLocation Result;
    const bool bFound =
        Navigation->GetRandomReachablePointInRadius(
            Origin,
            Radius,
            Result,
            NavData,
            QueryFilter);

    if (!bFound)
    {
        OutError = EGamePlatformNavigationError::PathNotFound;
        return false;
    }

    OutLocation = Result.Location;
    return true;
}

bool UGamePlatformNavigationWorldSubsystem::RegisterInvoker(
    AActor& Actor,
    FName AgentProfileId,
    EGamePlatformNavigationError& OutError)
{
    OutError = EGamePlatformNavigationError::None;

    const TWeakObjectPtr<AActor> ActorKey(&Actor);
    if (RegisteredInvokers.Contains(ActorKey))
    {
        return true;
    }

    UGamePlatformNavigationAgentProfile* Profile = nullptr;
    if (const TWeakObjectPtr<UGamePlatformNavigationAgentProfile>* Found =
        AgentProfiles.Find(AgentProfileId))
    {
        Profile = Found->Get();
    }

    if (!IsValid(Profile) ||
        Profile->InvokerPolicy !=
            EGamePlatformNavigationInvokerPolicy::RegisterWhenActive)
    {
        OutError =
            EGamePlatformNavigationError::InvokerRegistrationFailed;
        return false;
    }

    FText Reason;
    if (!Profile->ValidateProfile(Reason))
    {
        OutError =
            EGamePlatformNavigationError::InvalidAgentProfile;
        return false;
    }

    const UGamePlatformNavigationSettings* Settings =
        GetDefault<UGamePlatformNavigationSettings>();

    if (Profile->TileGenerationRadius >
            Settings->MaxInvokerGenerationRadius ||
        Profile->TileRemovalRadius >
            Settings->MaxInvokerRemovalRadius)
    {
        OutError =
            EGamePlatformNavigationError::InvokerRegistrationFailed;
        return false;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        OutError =
            EGamePlatformNavigationError::NavigationUnavailable;
        return false;
    }

    Navigation->RegisterNavigationInvoker(
        &Actor,
        Profile->TileGenerationRadius,
        Profile->TileRemovalRadius);

    RegisteredInvokers.Add(ActorKey);
    Actor.OnDestroyed.AddUniqueDynamic(
        this,
        &UGamePlatformNavigationWorldSubsystem::HandleInvokerDestroyed);

    return true;
}

void UGamePlatformNavigationWorldSubsystem::UnregisterInvoker(
    AActor& Actor)
{
    const TWeakObjectPtr<AActor> Key(&Actor);
    if (!RegisteredInvokers.Contains(Key))
    {
        return;
    }

    if (UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        Navigation->UnregisterNavigationInvoker(&Actor);
    }

    Actor.OnDestroyed.RemoveDynamic(
        this,
        &UGamePlatformNavigationWorldSubsystem::HandleInvokerDestroyed);

    RegisteredInvokers.Remove(Key);
}

FGamePlatformNavigationDiagnostics
UGamePlatformNavigationWorldSubsystem::GetDiagnostics() const
{
    FGamePlatformNavigationDiagnostics Result;
    Result.WorldGeneration = WorldGeneration;
    Result.OutstandingAsyncRequests = AsyncRequests.Num();
    Result.RegisteredInvokers = RegisteredInvokers.Num();
    Result.SyncQueryCount = SyncQueryCount;
    Result.AsyncQueryCount = AsyncQueryCount;
    Result.CancelledRequestCount = CancelledRequestCount;
    return Result;
}

bool UGamePlatformNavigationWorldSubsystem::RegisterAgentProfile(
    UGamePlatformNavigationAgentProfile* Profile)
{
    if (!IsValid(Profile) ||
        Profile->ProfileId.IsNone() ||
        AgentProfiles.Contains(Profile->ProfileId))
    {
        return false;
    }

    FText Reason;
    if (!Profile->ValidateProfile(Reason))
    {
        return false;
    }

    AgentProfiles.Add(Profile->ProfileId, Profile);
    return true;
}

bool UGamePlatformNavigationWorldSubsystem::RegisterFilter(
    FName FilterId,
    TSubclassOf<UNavigationQueryFilter> FilterClass)
{
    if (FilterId.IsNone() ||
        !FilterClass ||
        Filters.Contains(FilterId))
    {
        return false;
    }

    Filters.Add(FilterId, FilterClass);
    return true;
}

bool UGamePlatformNavigationWorldSubsystem::ValidateAgentProfileForActor(
    const AActor& Actor,
    FName AgentProfileId,
    FText& OutReason) const
{
    const ACharacter* Character = Cast<ACharacter>(&Actor);
    if (!Character || !Character->GetCapsuleComponent())
    {
        OutReason = FText::FromString(TEXT("Actor不是可校验的Character"));
        return false;
    }

    float ProfileRadius = 0.0f;
    float ProfileHeight = 0.0f;

    if (AgentProfileId.IsNone() ||
        AgentProfileId == DefaultAgentProfileId())
    {
        const FNavAgentProperties& DefaultAgent =
            UNavigationSystemV1::GetDefaultSupportedAgent();
        ProfileRadius = DefaultAgent.AgentRadius;
        ProfileHeight = DefaultAgent.AgentHeight;
    }
    else
    {
        const TWeakObjectPtr<UGamePlatformNavigationAgentProfile>* Found =
            AgentProfiles.Find(AgentProfileId);
        const UGamePlatformNavigationAgentProfile* Profile =
            Found ? Found->Get() : nullptr;

        if (!IsValid(Profile))
        {
            OutReason = FText::FromString(TEXT("Navigation Agent Profile未注册"));
            return false;
        }

        ProfileRadius = Profile->AgentRadius;
        ProfileHeight = Profile->AgentHeight;
    }

    const float CapsuleRadius =
        Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CapsuleHeight =
        Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;

    float RequiredRadius = CapsuleRadius;
    float RequiredHeight = CapsuleHeight;

    if (const UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement())
    {
        const FNavAgentProperties& CharacterAgent =
            Movement->GetNavAgentPropertiesRef();

        if (CharacterAgent.AgentRadius > 0.0f)
        {
            RequiredRadius =
                FMath::Max(RequiredRadius, CharacterAgent.AgentRadius);
        }

        if (CharacterAgent.AgentHeight > 0.0f)
        {
            RequiredHeight =
                FMath::Max(RequiredHeight, CharacterAgent.AgentHeight);
        }
    }

    if (!FMath::IsFinite(ProfileRadius) ||
        !FMath::IsFinite(ProfileHeight) ||
        ProfileRadius + KINDA_SMALL_NUMBER < RequiredRadius ||
        ProfileHeight + KINDA_SMALL_NUMBER < RequiredHeight)
    {
        OutReason = FText::Format(
            NSLOCTEXT(
                "GamePlatformNavigation",
                "AgentProfileTooSmall",
                "Navigation Profile({0},{1})小于Character所需({2},{3})"),
            FText::AsNumber(ProfileRadius),
            FText::AsNumber(ProfileHeight),
            FText::AsNumber(RequiredRadius),
            FText::AsNumber(RequiredHeight));
        return false;
    }

    OutReason = FText::GetEmpty();
    return true;
}

bool UGamePlatformNavigationWorldSubsystem::ResolveQueryContext(
    FName AgentProfileId,
    FName FilterId,
    const FVector& Location,
    FNavAgentProperties& OutAgent,
    ANavigationData*& OutNavData,
    FSharedConstNavQueryFilter& OutFilter,
    EGamePlatformNavigationError& OutError) const
{
    OutError = EGamePlatformNavigationError::None;
    OutNavData = nullptr;
    OutFilter.Reset();

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (!Navigation)
    {
        OutError =
            EGamePlatformNavigationError::NavigationUnavailable;
        return false;
    }

    if (AgentProfileId.IsNone() ||
        AgentProfileId == DefaultAgentProfileId())
    {
        OutAgent = UNavigationSystemV1::GetDefaultSupportedAgent();
    }
    else
    {
        const TWeakObjectPtr<UGamePlatformNavigationAgentProfile>* Found =
            AgentProfiles.Find(AgentProfileId);

        UGamePlatformNavigationAgentProfile* Profile =
            Found ? Found->Get() : nullptr;

        if (!IsValid(Profile))
        {
            OutError =
                EGamePlatformNavigationError::InvalidAgentProfile;
            return false;
        }

        FText Reason;
        if (!Profile->ValidateProfile(Reason))
        {
            OutError =
                EGamePlatformNavigationError::InvalidAgentProfile;
            return false;
        }

        OutAgent =
            FNavAgentProperties(
                Profile->AgentRadius,
                Profile->AgentHeight);
        OutAgent.AgentStepHeight = Profile->StepHeight;
    }

    OutNavData =
        Navigation->GetNavDataForProps(
            OutAgent,
            Location,
            OutAgent.GetExtent());

    if (!OutNavData)
    {
        OutError =
            EGamePlatformNavigationError::NavDataMissing;
        return false;
    }

    const FName EffectiveFilterId =
        FilterId.IsNone()
            ? DefaultFilterId()
            : FilterId;

    const TSubclassOf<UNavigationQueryFilter>* FilterClass =
        Filters.Find(EffectiveFilterId);

    if (!FilterClass || !(*FilterClass))
    {
        OutError =
            EGamePlatformNavigationError::InvalidFilter;
        return false;
    }

    OutFilter =
        OutNavData->GetQueryFilter(*FilterClass);

    if (!OutFilter.IsValid())
    {
        OutError = EGamePlatformNavigationError::InvalidFilter;
        return false;
    }

    return true;
}

bool UGamePlatformNavigationWorldSubsystem::BuildPathQuery(
    const FGamePlatformNavigationRequest& Request,
    FGuid EffectiveRequestId,
    FNavAgentProperties& OutAgent,
    FPathFindingQuery& OutQuery,
    FVector& OutResolvedStart,
    FVector& OutResolvedGoal,
    EGamePlatformNavigationError& OutError) const
{
    OutError = EGamePlatformNavigationError::None;

    if (!GetWorld())
    {
        OutError = EGamePlatformNavigationError::InvalidWorld;
        return false;
    }

    if (Request.WorldGeneration != 0 &&
        Request.WorldGeneration != WorldGeneration)
    {
        OutError =
            EGamePlatformNavigationError::StaleWorldGeneration;
        return false;
    }

    if (!IsFiniteVector(Request.Start))
    {
        OutError = EGamePlatformNavigationError::InvalidStart;
        return false;
    }

    if (!IsFiniteVector(Request.Goal))
    {
        OutError = EGamePlatformNavigationError::InvalidGoal;
        return false;
    }

    const UGamePlatformNavigationSettings* Settings =
        GetDefault<UGamePlatformNavigationSettings>();

    if (FVector::Distance(Request.Start, Request.Goal) >
        Settings->MaxPathDistance)
    {
        OutError = EGamePlatformNavigationError::InvalidGoal;
        return false;
    }

    ANavigationData* NavData = nullptr;
    FSharedConstNavQueryFilter QueryFilter;

    if (!ResolveQueryContext(
            Request.AgentProfileId,
            Request.FilterId,
            Request.Start,
            OutAgent,
            NavData,
            QueryFilter,
            OutError))
    {
        return false;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    FNavLocation ProjectedStart;
    FNavLocation ProjectedGoal;
    const FVector Extent = OutAgent.GetExtent();

    if (!Navigation->ProjectPointToNavigation(
            Request.Start,
            ProjectedStart,
            Extent,
            NavData,
            QueryFilter))
    {
        OutError = EGamePlatformNavigationError::InvalidStart;
        return false;
    }

    if (!Navigation->ProjectPointToNavigation(
            Request.Goal,
            ProjectedGoal,
            Extent,
            NavData,
            QueryFilter))
    {
        OutError = EGamePlatformNavigationError::InvalidGoal;
        return false;
    }

    OutResolvedStart = ProjectedStart.Location;
    OutResolvedGoal = ProjectedGoal.Location;

    const UObject* Owner =
        IsValid(Request.OwnerScope.Get())
            ? Request.OwnerScope.Get()
            : this;

    OutQuery =
        FPathFindingQuery(
            Owner,
            *NavData,
            OutResolvedStart,
            OutResolvedGoal,
            QueryFilter,
            nullptr,
            TNumericLimits<FVector::FReal>::Max(),
            false);

    OutQuery.SetNavAgentProperties(OutAgent);
    OutQuery.SetAllowPartialPaths(
        Request.PartialPathPolicy !=
        EGamePlatformNavigationPartialPathPolicy::RejectPartial);

    return true;
}

FGamePlatformNavigationPathResult
UGamePlatformNavigationWorldSubsystem::BuildPathResult(
    const FGamePlatformNavigationRequest& Request,
    FGuid EffectiveRequestId,
    int32 ExpectedWorldGeneration,
    ENavigationQueryResult::Type QueryResult,
    const FNavPathSharedPtr& Path,
    const FVector& ResolvedStart,
    const FVector& ResolvedGoal) const
{
    FGamePlatformNavigationPathResult Result;
    Result.RequestId = EffectiveRequestId;
    Result.WorldGeneration = ExpectedWorldGeneration;
    Result.ResolvedStart = ResolvedStart;
    Result.ResolvedGoal = ResolvedGoal;

    if (ExpectedWorldGeneration != WorldGeneration)
    {
        Result.Status =
            EGamePlatformNavigationPathStatus::Failed;
        Result.Error =
            EGamePlatformNavigationError::StaleWorldGeneration;
        return Result;
    }

    if (QueryResult != ENavigationQueryResult::Success ||
        !Path.IsValid())
    {
        Result.Status =
            EGamePlatformNavigationPathStatus::Failed;
        Result.Error =
            EGamePlatformNavigationError::PathNotFound;
        return Result;
    }

    Result.bIsPartial = Path->IsPartial();

    if (Result.bIsPartial &&
        Request.PartialPathPolicy ==
            EGamePlatformNavigationPartialPathPolicy::RejectPartial)
    {
        Result.Status =
            EGamePlatformNavigationPathStatus::Failed;
        Result.Error =
            EGamePlatformNavigationError::PartialPathRejected;
        return Result;
    }

    const TArray<FNavPathPoint>& EnginePoints =
        Path->GetPathPoints();

    Result.PathPoints.Reserve(EnginePoints.Num());

    FVector Previous = FVector::ZeroVector;
    bool bHasPrevious = false;

    for (const FNavPathPoint& Point : EnginePoints)
    {
        FGamePlatformNavigationPathPoint& OutPoint =
            Result.PathPoints.AddDefaulted_GetRef();

        OutPoint.Location = Point.Location;

        if (bHasPrevious)
        {
            Result.PathLength +=
                FVector::Distance(
                    Previous,
                    Point.Location);
        }

        Previous = Point.Location;
        bHasPrevious = true;
    }

    Result.PathCost =
        static_cast<float>(Path->GetCost());

    Result.Status =
        Result.bIsPartial
            ? EGamePlatformNavigationPathStatus::Partial
            : EGamePlatformNavigationPathStatus::Success;

    Result.Error = EGamePlatformNavigationError::None;
    Result.bReachedGoal = !Result.bIsPartial;
    return Result;
}

void UGamePlatformNavigationWorldSubsystem::HandleAsyncPathCompleted(
    uint32 QueryId,
    ENavigationQueryResult::Type QueryResult,
    FNavPathSharedPtr Path,
    FGuid RequestId,
    int32 ExpectedWorldGeneration,
    FVector ResolvedStart,
    FVector ResolvedGoal)
{
    FAsyncRequestRecord* Found =
        AsyncRequests.Find(RequestId);

    if (!Found ||
        Found->EngineQueryId != QueryId)
    {
        return;
    }

    FAsyncRequestRecord Record = MoveTemp(*Found);
    AsyncRequests.Remove(RequestId);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            Record.TimeoutHandle);
    }

    if (ExpectedWorldGeneration != WorldGeneration ||
        (Record.bHadOwner && !Record.Owner.IsValid()))
    {
        return;
    }

    const FGamePlatformNavigationPathResult Result =
        BuildPathResult(
            Record.Request,
            RequestId,
            ExpectedWorldGeneration,
            QueryResult,
            Path,
            ResolvedStart,
            ResolvedGoal);

    Record.Completion.ExecuteIfBound(Result);
}

void UGamePlatformNavigationWorldSubsystem::HandleAsyncTimeout(
    FGuid RequestId,
    int32 ExpectedWorldGeneration)
{
    FAsyncRequestRecord* Found =
        AsyncRequests.Find(RequestId);

    if (!Found ||
        Found->WorldGeneration != ExpectedWorldGeneration)
    {
        return;
    }

    FAsyncRequestRecord Record = MoveTemp(*Found);
    AsyncRequests.Remove(RequestId);

    if (UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        if (Record.EngineQueryId != 0)
        {
            Navigation->AbortAsyncFindPathRequest(
                Record.EngineQueryId);
        }
    }

    FGamePlatformNavigationPathResult Result;
    Result.RequestId = RequestId;
    Result.WorldGeneration = ExpectedWorldGeneration;
    Result.Status = EGamePlatformNavigationPathStatus::TimedOut;
    Result.Error = EGamePlatformNavigationError::RequestTimedOut;

    Record.Completion.ExecuteIfBound(Result);
}

void UGamePlatformNavigationWorldSubsystem::HandleInvokerDestroyed(
    AActor* DestroyedActor)
{
    if (DestroyedActor)
    {
        UnregisterInvoker(*DestroyedActor);
    }
}
