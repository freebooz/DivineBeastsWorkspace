#include "Components/GamePlatformInteractorComponent.h"

#include "Components/GamePlatformInteractableComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/GamePlatformGameplayEligibilityProvider.h"
#include "Interfaces/GamePlatformInteractionEligibilityProvider.h"
#include "Net/UnrealNetwork.h"
#include "Services/GamePlatformInteractionFocusRules.h"
#include "Settings/GamePlatformInteractionSettings.h"
#include "TimerManager.h"

namespace
{
template <typename TInterface>
const TInterface* FindProviderOnActor(const AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return nullptr;
    }

    if (const TInterface* Direct = Cast<TInterface>(Actor))
    {
        return Direct;
    }

    TInlineComponentArray<UActorComponent*> Components;
    const_cast<AActor*>(Actor)->GetComponents(Components);

    for (const UActorComponent* Component : Components)
    {
        if (const TInterface* Provider =
            Cast<TInterface>(Component))
        {
            return Provider;
        }
    }

    return nullptr;
}

template <typename TInterface>
const TInterface* FindNativeProvider(const AActor* Owner)
{
    if (const TInterface* OwnerProvider =
        FindProviderOnActor<TInterface>(Owner))
    {
        return OwnerProvider;
    }

    const APawn* Pawn = Cast<APawn>(Owner);
    const APlayerController* Controller =
        Cast<APlayerController>(Owner);

    if (Pawn)
    {
        Controller =
            Cast<APlayerController>(Pawn->GetController());
    }

    if (const TInterface* ControllerProvider =
        FindProviderOnActor<TInterface>(Controller))
    {
        return ControllerProvider;
    }

    if (Controller)
    {
        if (const TInterface* PlayerStateProvider =
            FindProviderOnActor<TInterface>(Controller->PlayerState))
        {
            return PlayerStateProvider;
        }
    }

    return nullptr;
}

APlayerController* ResolvePlayerController(const AActor* Owner)
{
    if (APlayerController* Controller =
        Cast<APlayerController>(const_cast<AActor*>(Owner)))
    {
        return Controller;
    }

    if (const APawn* Pawn = Cast<APawn>(Owner))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }

    return nullptr;
}

APawn* ResolveControlledPawn(const AActor* Owner)
{
    if (APawn* Pawn = Cast<APawn>(const_cast<AActor*>(Owner)))
    {
        return Pawn;
    }

    if (const APlayerController* Controller =
        Cast<APlayerController>(Owner))
    {
        return Controller->GetPawn();
    }

    return nullptr;
}
}

UGamePlatformInteractorComponent::UGamePlatformInteractorComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformInteractorComponent::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocallyControlledOwner() &&
        GetWorld() &&
        GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        const UGamePlatformInteractionSettings* Settings =
            GetDefault<UGamePlatformInteractionSettings>();

        GetWorld()->GetTimerManager().SetTimer(
            FocusTimer,
            this,
            &UGamePlatformInteractorComponent::RefreshLocalFocus,
            FMath::Max(0.02f, Settings->FocusRefreshInterval),
            true);

        RefreshLocalFocus();
    }
}

void UGamePlatformInteractorComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FocusTimer);
        World->GetTimerManager().ClearTimer(HoldValidationTimer);
    }

    if (GetOwner() && GetOwner()->HasAuthority() && IsSessionActive())
    {
        CancelSession(
            EndPlayReason == EEndPlayReason::LevelTransition
                ? EGamePlatformInteractionCancelReason::WorldTearingDown
                : EGamePlatformInteractionCancelReason::InteractorInvalid,
            EndPlayReason == EEndPlayReason::LevelTransition
                ? EGamePlatformInteractionError::WorldTearingDown
                : EGamePlatformInteractionError::InvalidInteractor);
    }

    CurrentFocus = {};
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformInteractorComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(
        UGamePlatformInteractorComponent,
        CurrentSession,
        COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(
        UGamePlatformInteractorComponent,
        LastResult,
        COND_OwnerOnly);
}

void UGamePlatformInteractorComponent::RefreshLocalFocus()
{
    if (!IsLocallyControlledOwner() || !GetWorld())
    {
        return;
    }

    FGamePlatformInteractionFocusSnapshot NewFocus;

    APlayerController* Controller = ResolvePlayerController(GetOwner());
    if (!IsValid(Controller))
    {
        if (CurrentFocus.IsValid())
        {
            CurrentFocus = {};
            OnFocusChanged.Broadcast(CurrentFocus);
        }
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();
    const FVector TraceEnd =
        ViewLocation +
        ViewRotation.Vector() * Settings->MaxConfiguredInteractionDistance;

    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(GamePlatformInteractionFocus),
        false);

    Params.AddIgnoredActor(GetOwner());
    if (APawn* Pawn = ResolveControlledPawn(GetOwner()))
    {
        Params.AddIgnoredActor(Pawn);
    }

    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        ViewLocation,
        TraceEnd,
        ECC_Visibility,
        Params);

    if (bHit && IsValid(Hit.GetActor()))
    {
        if (UGamePlatformInteractableComponent* Target =
            Hit.GetActor()->FindComponentByClass<
                UGamePlatformInteractableComponent>())
        {
            const float Distance =
                FVector::Distance(
                    GetServerInteractorOrigin(),
                    Target->GetInteractionPoint());

            FGamePlatformInteractionOption BestOption;
            if (Target->IsInteractionEnabled() &&
                FGamePlatformInteractionFocusRules::SelectBestOption(
                    Target->GetOptions(),
                    Distance,
                    BestOption))
            {
                NewFocus.TargetActor = Hit.GetActor();
                NewFocus.TargetInstanceId = Target->GetTargetInstanceId();
                NewFocus.TargetGeneration = Target->GetTargetGeneration();
                NewFocus.TargetRevision = Target->GetTargetRevision();
                NewFocus.Option = BestOption;
                NewFocus.Distance = Distance;
                NewFocus.bLocallyAvailable = true;
            }
        }
    }

    const bool bChanged =
        CurrentFocus.TargetActor != NewFocus.TargetActor ||
        CurrentFocus.Option.OptionId != NewFocus.Option.OptionId ||
        CurrentFocus.TargetRevision != NewFocus.TargetRevision ||
        CurrentFocus.bLocallyAvailable != NewFocus.bLocallyAvailable;

    if (bChanged)
    {
        CurrentFocus = NewFocus;
        OnFocusChanged.Broadcast(CurrentFocus);

        FGamePlatformInteractionEvent FocusEvent;
        FocusEvent.EventType =
            EGamePlatformInteractionEventType::FocusChanged;
        FocusEvent.InteractorActor = GetOwner();
        FocusEvent.TargetActor = CurrentFocus.TargetActor;
        FocusEvent.OptionId = CurrentFocus.Option.OptionId;
        OnInteractionEvent.Broadcast(FocusEvent);
    }
    else
    {
        CurrentFocus.Distance = NewFocus.Distance;
    }
}

FGuid UGamePlatformInteractorComponent::BeginFocusedInteraction()
{
    if (!ConsumeLocalRequestThrottle(true))
    {
        FGamePlatformInteractionResult Result;
        Result.Error = EGamePlatformInteractionError::RateLimited;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        LastResult = Result;
        OnResultChanged.Broadcast(LastResult);
        return {};
    }

    RefreshLocalFocus();

    if (!CurrentFocus.IsValid() ||
        !CurrentFocus.bLocallyAvailable)
    {
        FGamePlatformInteractionResult Result;
        Result.Error = EGamePlatformInteractionError::NoFocusedTarget;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        LastResult = Result;
        OnResultChanged.Broadcast(LastResult);
        return {};
    }

    FGamePlatformInteractionRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.TargetActor = CurrentFocus.TargetActor;
    Request.TargetInstanceId = CurrentFocus.TargetInstanceId;
    Request.TargetGeneration = CurrentFocus.TargetGeneration;
    Request.ObservedTargetRevision = CurrentFocus.TargetRevision;
    Request.OptionId = CurrentFocus.Option.OptionId;

    ServerRequestBeginInteraction(Request);
    return Request.RequestId;
}

void UGamePlatformInteractorComponent::CancelCurrentInteraction()
{
    if (!ConsumeLocalRequestThrottle(false))
    {
        return;
    }

    if (!CurrentSession.RequestId.IsValid())
    {
        return;
    }

    ServerRequestCancelInteraction(CurrentSession.RequestId);
}

float UGamePlatformInteractorComponent::GetHoldProgress() const
{
    if (CurrentSession.Mode != EGamePlatformInteractionMode::Hold ||
        CurrentSession.RequiredDuration <= KINDA_SMALL_NUMBER ||
        CurrentSession.State != EGamePlatformInteractionSessionState::Active)
    {
        return CurrentSession.State ==
            EGamePlatformInteractionSessionState::Completed
                ? 1.0f
                : 0.0f;
    }

    double ServerTime = 0.0;
    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GameState = World->GetGameState())
        {
            ServerTime = GameState->GetServerWorldTimeSeconds();
        }
        else
        {
            ServerTime = World->GetTimeSeconds();
        }
    }

    return FMath::Clamp(
        static_cast<float>(
            (ServerTime - CurrentSession.ServerStartTime) /
            CurrentSession.RequiredDuration),
        0.0f,
        1.0f);
}

void UGamePlatformInteractorComponent::CancelFromTarget(
    const FGuid& SessionId,
    EGamePlatformInteractionCancelReason Reason)
{
    if (!GetOwner()->HasAuthority() ||
        CurrentSession.SessionId != SessionId ||
        !IsSessionActive())
    {
        return;
    }

    const EGamePlatformInteractionError Error =
        Reason == EGamePlatformInteractionCancelReason::TargetDestroyed
            ? EGamePlatformInteractionError::TargetDestroyed
            : EGamePlatformInteractionError::TargetUnavailable;

    CancelSession(Reason, Error);
}

void UGamePlatformInteractorComponent::ServerRequestBeginInteraction_Implementation(
    FGamePlatformInteractionRequest Request)
{
    PruneRecentRequests();

    if (!ConsumeRequestRateLimit(true))
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = Request.RequestId;
        Result.Error = EGamePlatformInteractionError::RateLimited;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        CacheTerminalResult(Result);
        return;
    }

    if (!Request.RequestId.IsValid())
    {
        FGamePlatformInteractionResult Result;
        Result.Error = EGamePlatformInteractionError::InvalidInteractor;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        CacheTerminalResult(Result);
        return;
    }

    if (const FCachedRequestResult* Cached =
        RecentRequestResults.Find(Request.RequestId))
    {
        LastResult = Cached->Result;
        OnResultChanged.Broadcast(LastResult);
        return;
    }

    if (IsSessionActive() &&
        CurrentSession.RequestId == Request.RequestId)
    {
        LastResult.RequestId = CurrentSession.RequestId;
        LastResult.SessionId = CurrentSession.SessionId;
        LastResult.Error = EGamePlatformInteractionError::None;
        LastResult.State = CurrentSession.State;
        OnResultChanged.Broadcast(LastResult);
        return;
    }

    if (IsSessionActive())
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = Request.RequestId;
        Result.Error = EGamePlatformInteractionError::TargetBusy;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        CacheTerminalResult(Result);
        return;
    }

    UGamePlatformInteractableComponent* Target = nullptr;
    FGamePlatformInteractionOption Option;
    int32 InteractorGeneration = 0;

    const EGamePlatformInteractionError Error =
        ValidateBeginRequest(
            Request,
            Target,
            Option,
            InteractorGeneration);

    if (Error != EGamePlatformInteractionError::None)
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = Request.RequestId;
        Result.Error = Error;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        CacheTerminalResult(Result);
        return;
    }

    StartSession(
        Request,
        *Target,
        Option,
        InteractorGeneration);
}

void UGamePlatformInteractorComponent::ServerRequestCancelInteraction_Implementation(
    FGuid RequestId)
{
    if (!ConsumeRequestRateLimit(false))
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = RequestId;
        Result.Error = EGamePlatformInteractionError::RateLimited;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        LastResult = Result;
        OnResultChanged.Broadcast(LastResult);
        return;
    }

    if (!IsSessionActive() ||
        CurrentSession.RequestId != RequestId)
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = RequestId;
        Result.Error = EGamePlatformInteractionError::SessionNotFound;
        Result.State = EGamePlatformInteractionSessionState::Rejected;
        LastResult = Result;
        OnResultChanged.Broadcast(LastResult);
        return;
    }

    CancelSession(
        EGamePlatformInteractionCancelReason::UserCancelled,
        EGamePlatformInteractionError::UserCancelled);
}

void UGamePlatformInteractorComponent::OnRep_CurrentSession()
{
    OnSessionChanged.Broadcast(CurrentSession);
}

void UGamePlatformInteractorComponent::OnRep_LastResult()
{
    OnResultChanged.Broadcast(LastResult);
}

bool UGamePlatformInteractorComponent::IsLocallyControlledOwner() const
{
    if (const APlayerController* Controller =
        ResolvePlayerController(GetOwner()))
    {
        return Controller->IsLocalController();
    }

    return false;
}

bool UGamePlatformInteractorComponent::ResolveServerGameplayEligibility(
    int32& OutInteractorGeneration) const
{
    OutInteractorGeneration = 0;

    const IGamePlatformGameplayEligibilityProvider* Provider =
        FindNativeProvider<IGamePlatformGameplayEligibilityProvider>(
            GetOwner());

    if (!Provider ||
        !Provider->IsServerPlayerActiveForGameplay())
    {
        return false;
    }

    OutInteractorGeneration =
        Provider->GetGameplayAvatarGeneration();

    return OutInteractorGeneration > 0;
}

bool UGamePlatformInteractorComponent::ResolveExtraEligibility() const
{
    const IGamePlatformInteractionEligibilityProvider* Provider =
        FindNativeProvider<IGamePlatformInteractionEligibilityProvider>(
            GetOwner());

    return !Provider || Provider->IsEligibleForInteraction();
}

FVector UGamePlatformInteractorComponent::GetServerInteractorOrigin() const
{
    if (APawn* Pawn = ResolveControlledPawn(GetOwner()))
    {
        return Pawn->GetActorLocation();
    }

    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UGamePlatformInteractorComponent::ValidateServerDistance(
    const UGamePlatformInteractableComponent& Target,
    const FGamePlatformInteractionOption& Option) const
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    const float AllowedDistance =
        FMath::Min(
            Option.MaxDistance,
            Settings->MaxConfiguredInteractionDistance);

    if (!FMath::IsFinite(AllowedDistance) ||
        AllowedDistance <= 0.0f)
    {
        return false;
    }

    return FVector::DistSquared(
        GetServerInteractorOrigin(),
        Target.GetInteractionPoint()) <=
        FMath::Square(AllowedDistance);
}

bool UGamePlatformInteractorComponent::ValidateServerLineOfSight(
    const UGamePlatformInteractableComponent& Target,
    const FGamePlatformInteractionOption& Option) const
{
    if (!Option.bRequireLineOfSight)
    {
        return true;
    }

    AActor* OwnerActor = GetOwner();
    AActor* TargetActor = Target.GetOwner();

    if (!IsValid(OwnerActor) ||
        !IsValid(TargetActor) ||
        !GetWorld())
    {
        return false;
    }

    FVector TraceStart;
    FRotator TraceRotation;

    if (APlayerController* Controller =
        ResolvePlayerController(OwnerActor))
    {
        Controller->GetPlayerViewPoint(TraceStart, TraceRotation);
    }
    else
    {
        OwnerActor->GetActorEyesViewPoint(
            TraceStart,
            TraceRotation);
    }

    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    if (FVector::DistSquared(
            TraceStart,
            GetServerInteractorOrigin()) >
        FMath::Square(Settings->MaxTraceOriginOffset))
    {
        TraceStart = GetServerInteractorOrigin();
    }

    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(GamePlatformInteractionServerLOS),
        false);

    Params.AddIgnoredActor(OwnerActor);
    if (APawn* Pawn = ResolveControlledPawn(OwnerActor))
    {
        Params.AddIgnoredActor(Pawn);
    }

    FHitResult Hit;
    const bool bHit =
        GetWorld()->LineTraceSingleByChannel(
            Hit,
            TraceStart,
            Target.GetInteractionPoint(),
            ECC_Visibility,
            Params);

    return !bHit || Hit.GetActor() == TargetActor;
}

EGamePlatformInteractionError
UGamePlatformInteractorComponent::ValidateBeginRequest(
    const FGamePlatformInteractionRequest& Request,
    UGamePlatformInteractableComponent*& OutTarget,
    FGamePlatformInteractionOption& OutOption,
    int32& OutInteractorGeneration) const
{
    OutTarget = nullptr;
    OutInteractorGeneration = 0;

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return EGamePlatformInteractionError::InvalidInteractor;
    }

    if (!ResolveServerGameplayEligibility(
            OutInteractorGeneration) ||
        !ResolveExtraEligibility())
    {
        return EGamePlatformInteractionError::InteractorNotActive;
    }

    if (!IsValid(Request.TargetActor) ||
        Request.TargetActor->GetWorld() != GetWorld())
    {
        return EGamePlatformInteractionError::InvalidTarget;
    }

    OutTarget =
        Request.TargetActor->FindComponentByClass<
            UGamePlatformInteractableComponent>();

    if (!IsValid(OutTarget))
    {
        return EGamePlatformInteractionError::InvalidTarget;
    }

    if (Request.TargetInstanceId !=
        OutTarget->GetTargetInstanceId())
    {
        return EGamePlatformInteractionError::InvalidTarget;
    }

    if (Request.TargetGeneration !=
        OutTarget->GetTargetGeneration())
    {
        return EGamePlatformInteractionError::StaleTargetGeneration;
    }

    if (Request.ObservedTargetRevision !=
        OutTarget->GetTargetRevision())
    {
        return EGamePlatformInteractionError::StaleTargetRevision;
    }

    const FGamePlatformInteractionOption* Option =
        OutTarget->FindOption(Request.OptionId);

    if (!Option)
    {
        return EGamePlatformInteractionError::InvalidOption;
    }

    OutOption = *Option;

    if (!OutTarget->IsInteractionEnabled() ||
        !OutOption.bEnabled ||
        !OutOption.IsStructurallyValid() ||
        (OutOption.CommitKind ==
             EGamePlatformInteractionCommitKind::Harvest &&
         OutTarget->GetRemainingCharges() <= 0))
    {
        return EGamePlatformInteractionError::TargetUnavailable;
    }

    if (!ValidateServerDistance(*OutTarget, OutOption))
    {
        return EGamePlatformInteractionError::OutOfRange;
    }

    if (!ValidateServerLineOfSight(*OutTarget, OutOption))
    {
        return EGamePlatformInteractionError::LineOfSightBlocked;
    }

    FGamePlatformInteractionSession Probe;
    Probe.InteractorActor = GetOwner();
    Probe.InteractorGeneration = OutInteractorGeneration;
    Probe.TargetActor = Request.TargetActor;
    Probe.TargetInstanceId = Request.TargetInstanceId;
    Probe.TargetGeneration = Request.TargetGeneration;
    Probe.TargetRevisionAtStart = Request.ObservedTargetRevision;
    Probe.OptionId = Request.OptionId;
    Probe.Mode = OutOption.Mode;

    if (const IGamePlatformInteractable* Interface =
        Cast<IGamePlatformInteractable>(Request.TargetActor))
    {
        if (!Interface->CanBeginInteraction(
                Probe,
                OutOption))
        {
            return EGamePlatformInteractionError::TargetUnavailable;
        }
    }

    return EGamePlatformInteractionError::None;
}

EGamePlatformInteractionError
UGamePlatformInteractorComponent::ValidateActiveSession() const
{
    if (!IsSessionActive())
    {
        return EGamePlatformInteractionError::SessionNotFound;
    }

    int32 CurrentGeneration = 0;
    if (!ResolveServerGameplayEligibility(CurrentGeneration) ||
        !ResolveExtraEligibility())
    {
        return EGamePlatformInteractionError::InteractorNotActive;
    }

    if (CurrentGeneration !=
        CurrentSession.InteractorGeneration)
    {
        return EGamePlatformInteractionError::InteractorNotActive;
    }

    UGamePlatformInteractableComponent* Target =
        GetCurrentTargetComponent();

    if (!IsValid(Target))
    {
        return EGamePlatformInteractionError::TargetDestroyed;
    }

    if (Target->GetTargetInstanceId() !=
        CurrentSession.TargetInstanceId)
    {
        return EGamePlatformInteractionError::InvalidTarget;
    }

    if (Target->GetTargetGeneration() !=
        CurrentSession.TargetGeneration)
    {
        return EGamePlatformInteractionError::StaleTargetGeneration;
    }

    if (Target->GetTargetRevision() !=
        CurrentSession.TargetRevisionAtStart)
    {
        return EGamePlatformInteractionError::StaleTargetRevision;
    }

    const FGamePlatformInteractionOption* Option =
        Target->FindOption(CurrentSession.OptionId);

    if (!Option ||
        !Target->CanContinueSession(
            CurrentSession.SessionId,
            *Option))
    {
        return EGamePlatformInteractionError::TargetUnavailable;
    }

    if (!ValidateServerDistance(*Target, *Option))
    {
        return EGamePlatformInteractionError::OutOfRange;
    }

    if (!ValidateServerLineOfSight(*Target, *Option))
    {
        return EGamePlatformInteractionError::LineOfSightBlocked;
    }

    return EGamePlatformInteractionError::None;
}

void UGamePlatformInteractorComponent::StartSession(
    const FGamePlatformInteractionRequest& Request,
    UGamePlatformInteractableComponent& Target,
    const FGamePlatformInteractionOption& Option,
    int32 InteractorGeneration)
{
    CurrentSession = {};
    CurrentSession.SessionId = FGuid::NewGuid();
    CurrentSession.RequestId = Request.RequestId;
    CurrentSession.InteractorActor = GetOwner();
    CurrentSession.InteractorGeneration = InteractorGeneration;
    CurrentSession.TargetActor = Request.TargetActor;
    CurrentSession.TargetInstanceId = Request.TargetInstanceId;
    CurrentSession.TargetGeneration = Request.TargetGeneration;
    CurrentSession.TargetRevisionAtStart =
        Request.ObservedTargetRevision;
    CurrentSession.OptionId = Request.OptionId;
    CurrentSession.Mode = Option.Mode;
    CurrentSession.RequiredDuration =
        Option.Mode == EGamePlatformInteractionMode::Hold
            ? Option.HoldDuration
            : 0.0f;

    CurrentSession.State =
        EGamePlatformInteractionSessionState::Validating;

    EGamePlatformInteractionError AcquireError =
        EGamePlatformInteractionError::None;

    if (!Target.TryAcquireSession(
            CurrentSession.SessionId,
            this,
            Option,
            AcquireError))
    {
        FGamePlatformInteractionResult Result;
        Result.RequestId = Request.RequestId;
        Result.SessionId = CurrentSession.SessionId;
        Result.Error = AcquireError;
        Result.State =
            EGamePlatformInteractionSessionState::Rejected;
        CurrentSession.Result = Result;
        FinishSession(
            Result,
            EGamePlatformInteractionEventType::InteractionCancelled);
        return;
    }

    CurrentSession.State =
        EGamePlatformInteractionSessionState::Active;

    if (const AGameStateBase* GameState =
        GetWorld()->GetGameState())
    {
        CurrentSession.ServerStartTime =
            GameState->GetServerWorldTimeSeconds();
    }
    else
    {
        CurrentSession.ServerStartTime =
            GetWorld()->GetTimeSeconds();
    }

    OnSessionChanged.Broadcast(CurrentSession);

    FGamePlatformInteractionResult Started;
    Started.RequestId = Request.RequestId;
    Started.SessionId = CurrentSession.SessionId;
    Started.State =
        EGamePlatformInteractionSessionState::Active;
    PublishEvent(
        EGamePlatformInteractionEventType::InteractionStarted,
        Started);

    if (Option.Mode ==
        EGamePlatformInteractionMode::Instant)
    {
        CommitCurrentSession();
        return;
    }

    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    GetWorld()->GetTimerManager().SetTimer(
        HoldValidationTimer,
        this,
        &UGamePlatformInteractorComponent::ValidateHoldSession,
        FMath::Max(0.05f, Settings->HoldValidationInterval),
        true);
}

void UGamePlatformInteractorComponent::CommitCurrentSession()
{
    if (!IsSessionActive())
    {
        return;
    }

    const EGamePlatformInteractionError Validation =
        ValidateActiveSession();

    if (Validation != EGamePlatformInteractionError::None)
    {
        EGamePlatformInteractionCancelReason Reason =
            EGamePlatformInteractionCancelReason::TargetUnavailable;

        if (Validation ==
            EGamePlatformInteractionError::OutOfRange)
        {
            Reason =
                EGamePlatformInteractionCancelReason::OutOfRange;
        }
        else if (Validation ==
            EGamePlatformInteractionError::LineOfSightBlocked)
        {
            Reason =
                EGamePlatformInteractionCancelReason::LineOfSightLost;
        }
        else if (Validation ==
            EGamePlatformInteractionError::StaleTargetRevision)
        {
            Reason =
                EGamePlatformInteractionCancelReason::TargetRevisionChanged;
        }
        else if (Validation ==
            EGamePlatformInteractionError::InteractorNotActive)
        {
            Reason =
                EGamePlatformInteractionCancelReason::InteractorNotActive;
        }

        CancelSession(Reason, Validation);
        return;
    }

    UGamePlatformInteractableComponent* Target =
        GetCurrentTargetComponent();

    const FGamePlatformInteractionOption* Option =
        Target
            ? Target->FindOption(CurrentSession.OptionId)
            : nullptr;

    if (!Target || !Option)
    {
        CancelSession(
            EGamePlatformInteractionCancelReason::TargetUnavailable,
            EGamePlatformInteractionError::TargetUnavailable);
        return;
    }

    CurrentSession.State =
        EGamePlatformInteractionSessionState::Committing;

    FGamePlatformInteractionResult Result;
    const bool bCommitted =
        Target->CommitSession(
            CurrentSession,
            *Option,
            Result);

    Target->ReleaseSession(CurrentSession.SessionId);
    GetWorld()->GetTimerManager().ClearTimer(HoldValidationTimer);

    if (!bCommitted)
    {
        Result.RequestId = CurrentSession.RequestId;
        Result.SessionId = CurrentSession.SessionId;
        Result.State =
            EGamePlatformInteractionSessionState::Failed;

        FinishSession(
            Result,
            EGamePlatformInteractionEventType::InteractionCancelled);
        return;
    }

    EGamePlatformInteractionEventType EventType =
        EGamePlatformInteractionEventType::InteractionCommitted;

    if (Option->CommitKind ==
        EGamePlatformInteractionCommitKind::Consume)
    {
        EventType =
            EGamePlatformInteractionEventType::PickupConsumed;
    }
    else if (Option->CommitKind ==
        EGamePlatformInteractionCommitKind::Harvest)
    {
        EventType =
            EGamePlatformInteractionEventType::HarvestCompleted;
    }

    FinishSession(Result, EventType);
}

void UGamePlatformInteractorComponent::ValidateHoldSession()
{
    if (!IsSessionActive() ||
        CurrentSession.Mode !=
            EGamePlatformInteractionMode::Hold)
    {
        GetWorld()->GetTimerManager().ClearTimer(
            HoldValidationTimer);
        return;
    }

    const EGamePlatformInteractionError Validation =
        ValidateActiveSession();

    if (Validation != EGamePlatformInteractionError::None)
    {
        CommitCurrentSession();
        return;
    }

    double ServerTime = GetWorld()->GetTimeSeconds();
    if (const AGameStateBase* GameState =
        GetWorld()->GetGameState())
    {
        ServerTime =
            GameState->GetServerWorldTimeSeconds();
    }

    if (ServerTime - CurrentSession.ServerStartTime >=
        CurrentSession.RequiredDuration)
    {
        CommitCurrentSession();
    }
}

void UGamePlatformInteractorComponent::CancelSession(
    EGamePlatformInteractionCancelReason Reason,
    EGamePlatformInteractionError Error)
{
    if (!IsSessionActive())
    {
        return;
    }

    if (UGamePlatformInteractableComponent* Target =
        GetCurrentTargetComponent())
    {
        Target->ReleaseSession(CurrentSession.SessionId);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            HoldValidationTimer);
    }

    FGamePlatformInteractionResult Result;
    Result.RequestId = CurrentSession.RequestId;
    Result.SessionId = CurrentSession.SessionId;
    Result.Error = Error;
    Result.CancelReason = Reason;
    Result.State =
        Reason == EGamePlatformInteractionCancelReason::TimedOut
            ? EGamePlatformInteractionSessionState::TimedOut
            : EGamePlatformInteractionSessionState::Cancelled;

    FinishSession(
        Result,
        EGamePlatformInteractionEventType::InteractionCancelled);
}

void UGamePlatformInteractorComponent::FinishSession(
    const FGamePlatformInteractionResult& Result,
    EGamePlatformInteractionEventType EventType)
{
    FGamePlatformInteractionResult FinalResult = Result;
    FinalResult.OptionId = CurrentSession.OptionId;

    LastResult = FinalResult;
    CurrentSession.State = FinalResult.State;
    CurrentSession.Result = FinalResult;

    CacheTerminalResult(FinalResult);
    PublishEvent(EventType, FinalResult);

    OnSessionChanged.Broadcast(CurrentSession);
}

void UGamePlatformInteractorComponent::PublishEvent(
    EGamePlatformInteractionEventType EventType,
    const FGamePlatformInteractionResult& Result)
{
    FGamePlatformInteractionEvent Event;
    Event.EventType = EventType;
    Event.RequestId = Result.RequestId;
    Event.SessionId = Result.SessionId;
    Event.InteractorActor = GetOwner();
    Event.TargetActor = CurrentSession.TargetActor;
    Event.OptionId = CurrentSession.OptionId;
    Event.Result = Result;

    OnInteractionEvent.Broadcast(Event);
}

bool UGamePlatformInteractorComponent::ConsumeLocalRequestThrottle(
    bool bBeginRequest)
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    const double Now =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    double& LastRequestTime =
        bBeginRequest
            ? LastLocalBeginRequestTime
            : LastLocalCancelRequestTime;

    const double MinimumInterval =
        FMath::Max(
            0.0f,
            Settings->MinClientRequestInterval);

    if (LastRequestTime >= 0.0 &&
        Now - LastRequestTime < MinimumInterval)
    {
        return false;
    }

    LastRequestTime = Now;
    return true;
}

bool UGamePlatformInteractorComponent::ConsumeRequestRateLimit(
    bool bBeginRequest)
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    const double Now =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    double& WindowStart =
        bBeginRequest
            ? BeginWindowStart
            : CancelWindowStart;

    int32& Count =
        bBeginRequest
            ? BeginRequestCount
            : CancelRequestCount;

    const int32 Limit =
        bBeginRequest
            ? Settings->MaxBeginRequestsPerWindow
            : Settings->MaxCancelRequestsPerWindow;

    if (Now - WindowStart >=
        Settings->BeginRequestWindowSeconds)
    {
        WindowStart = Now;
        Count = 0;
    }

    ++Count;
    return Count <= FMath::Max(1, Limit);
}

void UGamePlatformInteractorComponent::PruneRecentRequests()
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    const double Now =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    while (!RecentRequestOrder.IsEmpty())
    {
        const FGuid OldestId = RecentRequestOrder[0];
        const FCachedRequestResult* Cached =
            RecentRequestResults.Find(OldestId);

        const bool bExpired =
            !Cached ||
            Now - Cached->RecordedAt >
                Settings->RecentRequestLifetimeSeconds;

        const bool bOverLimit =
            RecentRequestOrder.Num() >
                FMath::Max(8, Settings->MaxRecentRequests);

        if (!bExpired && !bOverLimit)
        {
            break;
        }

        RecentRequestOrder.RemoveAt(
            0,
            1,
            EAllowShrinking::No);
        RecentRequestResults.Remove(OldestId);
    }
}

void UGamePlatformInteractorComponent::CacheTerminalResult(
    const FGamePlatformInteractionResult& Result)
{
    if (!Result.RequestId.IsValid())
    {
        LastResult = Result;
        OnResultChanged.Broadcast(LastResult);
        return;
    }

    FCachedRequestResult Cached;
    Cached.Result = Result;
    Cached.RecordedAt =
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    if (!RecentRequestResults.Contains(
            Result.RequestId))
    {
        RecentRequestOrder.Add(Result.RequestId);
    }

    RecentRequestResults.Add(
        Result.RequestId,
        Cached);

    LastResult = Result;
    OnResultChanged.Broadcast(LastResult);
    PruneRecentRequests();
}

UGamePlatformInteractableComponent*
UGamePlatformInteractorComponent::GetCurrentTargetComponent() const
{
    if (!IsValid(CurrentSession.TargetActor))
    {
        return nullptr;
    }

    return CurrentSession.TargetActor
        ->FindComponentByClass<
            UGamePlatformInteractableComponent>();
}

bool UGamePlatformInteractorComponent::IsSessionActive() const
{
    return CurrentSession.SessionId.IsValid() &&
           (CurrentSession.State ==
                EGamePlatformInteractionSessionState::Active ||
            CurrentSession.State ==
                EGamePlatformInteractionSessionState::Validating ||
            CurrentSession.State ==
                EGamePlatformInteractionSessionState::Committing);
}
