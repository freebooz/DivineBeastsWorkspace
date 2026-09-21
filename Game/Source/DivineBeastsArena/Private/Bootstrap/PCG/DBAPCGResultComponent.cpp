#include "Bootstrap/PCG/DBAPCGResultComponent.h"
#include "Interfaces/IGamePlatformPCGService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAPCGOwnership, Log, All);

namespace
{
FGamePlatformResult BridgeFailure(FName Code)
{
    return FGamePlatformResult::Failure(Code, TEXT("PCG项目桥接的世界、区域、实例或请求所有权不再有效；不代表已清理。"));
}
}

UDBAPCGResultComponent::UDBAPCGResultComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

FGamePlatformResult UDBAPCGResultComponent::BindWorldRegion(const FGuid& ContextGeneration,
    const FGamePlatformId& RegionId, const FVector& CenterCm)
{
    check(IsInGameThread());
    if (bIsBound || bIsStopping) { return BridgeFailure(TEXT("PCGOwnerAlreadyBoundOrStopping")); }
    if (!ContextGeneration.IsValid() || !RegionId.IsValid() ||
        !FMath::IsFinite(CenterCm.X) || !FMath::IsFinite(CenterCm.Y) || !FMath::IsFinite(CenterCm.Z))
    { return BridgeFailure(TEXT("PCGInvalidBinding")); }
    BoundWorld = GetWorld();
    BoundGeneration = ContextGeneration;
    BoundRegion = RegionId;
    BoundCenterCm = CenterCm;
    const auto Result = ValidateBoundContext();
    if (Result.IsSuccess()) { bIsBound = true; }
    else { BoundWorld.Reset(); BoundGeneration.Invalidate(); BoundRegion = {}; }
    return Result;
}

FGamePlatformResult UDBAPCGResultComponent::ValidateBoundContext() const
{
    auto* World = BoundWorld.Get();
    const auto* Actor = GetOwner();
    if (bIsStopping || !IsRegistered() || !IsValid(Actor) || Actor->IsActorBeingDestroyed() ||
        !World || GetWorld() != World || World->bIsTearingDown || IsRunningCommandlet() ||
        (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
    { return BridgeFailure(TEXT("PCGOwnerWorldExpired")); }
    auto* Instance = World->GetGameInstance();
    if (!Instance || Instance->GetWorld() != World) { return BridgeFailure(TEXT("PCGOwnerInstanceMismatch")); }
    auto* Service = IGamePlatformWorldService::Get(*World);
    if (!Service) { return BridgeFailure(TEXT("PCGWorldServiceMissing")); }
    const auto Snapshot = Service->GetReadiness();
    if (Snapshot.Context.ContextGeneration != BoundGeneration || !Snapshot.Context.WorldId.IsValid() ||
        !Snapshot.bWorldObjectValid || !Snapshot.bDefinitionLoaded || !Snapshot.bMapIdentityMatched ||
        !Snapshot.bSessionContextMatched || !Snapshot.bWorldNotTearingDown ||
        Snapshot.Context.ReadinessState == EGamePlatformWorldReadiness::Failed ||
        Snapshot.Context.ReadinessState == EGamePlatformWorldReadiness::Invalidated)
    { return BridgeFailure(TEXT("PCGBaseWorldInputsUnavailable")); }
    FGamePlatformId ActualRegion;
    const auto Queried = Service->QueryRegion(BoundCenterCm, ActualRegion);
    if (!Queried.IsSuccess()) { return Queried; }
    if (ActualRegion != BoundRegion) { return BridgeFailure(TEXT("PCGRegionInvalidated")); }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UDBAPCGResultComponent::StartGeneration(UGameInstance& Instance,
    const FPrimaryAssetId& ProfileId, FGamePlatformPCGHandle& OutHandle)
{
    check(IsInGameThread());
    OutHandle = {};
    if (!bIsBound) { return BridgeFailure(TEXT("PCGOwnerNotBound")); }
    const auto Context = ValidateBoundContext();
    if (!Context.IsSuccess()) { return Context; }
    if (BoundWorld->GetGameInstance() != &Instance) { return BridgeFailure(TEXT("PCGTaskInstanceMismatch")); }
    auto* Service = IGamePlatformPCGService::Get(*BoundWorld.Get());
    if (!Service) { return BridgeFailure(TEXT("PCGServiceMissing")); }
    if (GenerationHandle.IsValid())
    {
        const auto Previous = Service->GetGenerationSnapshot(GenerationHandle);
        if (!(Previous.Handle == GenerationHandle) || Previous.Phase != EGamePlatformPCGPhase::Cleaned)
        { return BridgeFailure(TEXT("PCGResultStillOwned")); }
    }
    FGamePlatformPCGRequest Request;
    Request.ProfileId = ProfileId;
    Request.RegionId = BoundRegion;
    Request.ContextGeneration = BoundGeneration;
    Request.CenterCm = BoundCenterCm;
    Request.Owner = this;
    FGamePlatformResult Accepted;
    const auto Handle = Service->RequestGeneration(Request, Accepted);
    if (!Accepted.IsSuccess()) { return Accepted; }
    if (!Handle.IsValid()) { return BridgeFailure(TEXT("PCGInvalidAcceptedHandle")); }
    GenerationHandle = Handle;
    OutHandle = Handle;
    return Accepted;
}

FGamePlatformPCGSnapshot UDBAPCGResultComponent::ReadGeneration(const FGamePlatformPCGHandle& Handle) const
{
    check(IsInGameThread());
    FGamePlatformPCGSnapshot Snapshot;
    if (!Handle.IsValid() || !(Handle == GenerationHandle))
    { Snapshot.Result = BridgeFailure(TEXT("PCGTaskHandleMismatch")); return Snapshot; }
    auto* World = BoundWorld.Get();
    auto* Service = World ? IGamePlatformPCGService::Get(*World) : nullptr;
    if (!Service) { Snapshot.Result = BridgeFailure(TEXT("PCGServiceMissing")); return Snapshot; }
    Snapshot = Service->GetGenerationSnapshot(Handle);
    const auto Context = ValidateBoundContext();
    if (!Context.IsSuccess()) { Snapshot.bIsResultValid = false; Snapshot.Result = Context; }
    return Snapshot;
}

FGamePlatformResult UDBAPCGResultComponent::ReleaseGeneration(const FGamePlatformPCGHandle& Handle)
{
    check(IsInGameThread());
    if (!Handle.IsValid() || !(Handle == GenerationHandle)) { return BridgeFailure(TEXT("PCGTaskHandleMismatch")); }
    auto* World = BoundWorld.Get();
    auto* Service = World ? IGamePlatformPCGService::Get(*World) : nullptr;
    return Service ? Service->ReleaseGeneration(Handle) : BridgeFailure(TEXT("PCGServiceMissing"));
}

FGamePlatformPCGHandle UDBAPCGResultComponent::GetGenerationHandle() const
{
    check(IsInGameThread());
    return GenerationHandle;
}

void UDBAPCGResultComponent::StopOwnedGeneration()
{
    bIsStopping = true;
    if (GenerationHandle.IsValid())
    {
        const auto Result = ReleaseGeneration(GenerationHandle);
        if (!Result.IsSuccess())
        {
            UE_LOG(LogDBAPCGOwnership, Warning, TEXT("PCG owner teardown release rejected Code=%s; PCG world/weak-owner cleanup remains authoritative"), *Result.Code.ToString());
        }
    }
}

void UDBAPCGResultComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopOwnedGeneration();
    Super::EndPlay(EndPlayReason);
}

void UDBAPCGResultComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    StopOwnedGeneration();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}
