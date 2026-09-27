#include "Components/GamePlatformInteractableComponent.h"

#include "Components/GamePlatformInteractorComponent.h"
#include "Interfaces/GamePlatformInteractable.h"
#include "Net/UnrealNetwork.h"
#include "Settings/GamePlatformInteractionSettings.h"
#include "Types/GamePlatformInteractionSession.h"

UGamePlatformInteractableComponent::UGamePlatformInteractableComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformInteractableComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner()->HasAuthority() && !TargetInstanceId.IsValid())
    {
        TargetInstanceId = FGuid::NewGuid();
        TargetGeneration = FMath::Max(1, TargetGeneration);
        TargetRevision = FMath::Max(1, TargetRevision);
    }
}

void UGamePlatformInteractableComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (GetOwner()->HasAuthority())
    {
        TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
        Sessions.Reserve(ActiveSessions.Num());
        for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
        {
            Sessions.Add(Pair);
        }

        ActiveSessions.Reset();
        OccupancyCount = 0;

        for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
        {
            if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
            {
                Interactor->CancelFromTarget(
                    Pair.Key,
                    EGamePlatformInteractionCancelReason::TargetDestroyed);
            }
        }

        ++TargetGeneration;
    }

    Super::EndPlay(EndPlayReason);
}

void UGamePlatformInteractableComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetInstanceId);
    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetGeneration);
    DOREPLIFETIME(UGamePlatformInteractableComponent, TargetRevision);
    DOREPLIFETIME(UGamePlatformInteractableComponent, Options);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bEnabled);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bConsumed);
    DOREPLIFETIME(UGamePlatformInteractableComponent, bToggleState);
    DOREPLIFETIME(UGamePlatformInteractableComponent, RemainingCharges);
    DOREPLIFETIME(UGamePlatformInteractableComponent, OccupancyCount);
}

const FGamePlatformInteractionOption*
UGamePlatformInteractableComponent::FindOption(FName OptionId) const
{
    return Options.FindByPredicate(
        [OptionId](const FGamePlatformInteractionOption& Option)
        {
            return Option.OptionId == OptionId;
        });
}

bool UGamePlatformInteractableComponent::IsOptionAvailable(
    const FGamePlatformInteractionOption& Option) const
{
    if (!IsInteractionEnabled() ||
        !Option.bEnabled ||
        !Option.IsStructurallyValid())
    {
        return false;
    }

    if (Option.CommitKind == EGamePlatformInteractionCommitKind::Harvest &&
        RemainingCharges <= 0)
    {
        return false;
    }

    if (Option.ConcurrencyPolicy ==
        EGamePlatformInteractionConcurrencyPolicy::Exclusive)
    {
        return OccupancyCount == 0;
    }

    return OccupancyCount < FMath::Max(1, Option.MaxConcurrent);
}

bool UGamePlatformInteractableComponent::CanContinueSession(
    const FGuid& SessionId,
    const FGamePlatformInteractionOption& Option) const
{
    if (!ActiveSessions.Contains(SessionId) ||
        !IsInteractionEnabled() ||
        !Option.bEnabled ||
        !Option.IsStructurallyValid())
    {
        return false;
    }

    if (Option.CommitKind == EGamePlatformInteractionCommitKind::Harvest &&
        RemainingCharges <= 0)
    {
        return false;
    }

    return true;
}
bool UGamePlatformInteractableComponent::SetOptions(
    const TArray<FGamePlatformInteractionOption>& InOptions)
{
    if (!GetOwner()->HasAuthority() || !ValidateOptions(InOptions))
    {
        return false;
    }

    Options = InOptions;
    BumpRevision();
    CancelActiveSessions(
        EGamePlatformInteractionCancelReason::TargetRevisionChanged);
    return true;
}

bool UGamePlatformInteractableComponent::SetInteractionEnabled(bool bInEnabled)
{
    if (!GetOwner()->HasAuthority() || bEnabled == bInEnabled)
    {
        return false;
    }

    bEnabled = bInEnabled;
    BumpRevision();
    if (!bEnabled)
    {
        CancelActiveSessions(
            EGamePlatformInteractionCancelReason::TargetUnavailable);
    }
    return true;
}

bool UGamePlatformInteractableComponent::SetRemainingCharges(
    int32 InRemainingCharges)
{
    if (!GetOwner()->HasAuthority())
    {
        return false;
    }

    const int32 NewValue = FMath::Max(0, InRemainingCharges);
    if (RemainingCharges == NewValue)
    {
        return false;
    }

    RemainingCharges = NewValue;
    if (RemainingCharges == 0)
    {
        bEnabled = false;
    }

    BumpRevision();
    CancelActiveSessions(
        RemainingCharges <= 0
            ? EGamePlatformInteractionCancelReason::TargetUnavailable
            : EGamePlatformInteractionCancelReason::TargetRevisionChanged);
    return true;
}

void UGamePlatformInteractableComponent::AdvanceTargetGeneration()
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }

    TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
    Sessions.Reserve(ActiveSessions.Num());
    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
    {
        Sessions.Add(Pair);
    }

    ++TargetGeneration;
    ActiveSessions.Reset();
    OccupancyCount = 0;
    CommittedSessions.Reset();
    CommittedSessionOrder.Reset();
    BumpRevision();

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
    {
        if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
        {
            Interactor->CancelFromTarget(
                Pair.Key,
                EGamePlatformInteractionCancelReason::TargetRevisionChanged);
        }
    }
}

bool UGamePlatformInteractableComponent::TryAcquireSession(
    const FGuid& SessionId,
    UGamePlatformInteractorComponent* Interactor,
    const FGamePlatformInteractionOption& Option,
    EGamePlatformInteractionError& OutError)
{
    OutError = EGamePlatformInteractionError::None;

    if (!GetOwner()->HasAuthority() ||
        !SessionId.IsValid() ||
        !IsValid(Interactor))
    {
        OutError = EGamePlatformInteractionError::InvalidInteractor;
        return false;
    }

    if (!IsOptionAvailable(Option))
    {
        OutError =
            IsInteractionEnabled()
                ? EGamePlatformInteractionError::TargetBusy
                : EGamePlatformInteractionError::TargetUnavailable;
        return false;
    }

    if (Option.ConcurrencyPolicy ==
        EGamePlatformInteractionConcurrencyPolicy::Exclusive)
    {
        if (OccupancyCount > 0)
        {
            OutError = EGamePlatformInteractionError::TargetBusy;
            return false;
        }
    }
    else if (OccupancyCount >= FMath::Max(1, Option.MaxConcurrent))
    {
        OutError = EGamePlatformInteractionError::ConcurrentLimitReached;
        return false;
    }

    ActiveSessions.Add(SessionId, Interactor);
    OccupancyCount = ActiveSessions.Num();
    return true;
}

void UGamePlatformInteractableComponent::ReleaseSession(const FGuid& SessionId)
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }

    ActiveSessions.Remove(SessionId);
    OccupancyCount = ActiveSessions.Num();
}

bool UGamePlatformInteractableComponent::CommitSession(
    const FGamePlatformInteractionSession& Session,
    const FGamePlatformInteractionOption& Option,
    FGamePlatformInteractionResult& OutResult)
{
    OutResult.RequestId = Session.RequestId;
    OutResult.SessionId = Session.SessionId;

    if (!GetOwner()->HasAuthority())
    {
        OutResult.Error = EGamePlatformInteractionError::InvalidTarget;
        return false;
    }

    if (CommittedSessions.Contains(Session.SessionId))
    {
        OutResult.Error = EGamePlatformInteractionError::SessionAlreadyCompleted;
        return false;
    }

    if (!ActiveSessions.Contains(Session.SessionId) ||
        Session.TargetGeneration != TargetGeneration ||
        Session.TargetRevisionAtStart != TargetRevision ||
        !CanContinueSession(Session.SessionId, Option))
    {
        OutResult.Error =
            Session.TargetGeneration != TargetGeneration
                ? EGamePlatformInteractionError::StaleTargetGeneration
                : Session.TargetRevisionAtStart != TargetRevision
                    ? EGamePlatformInteractionError::StaleTargetRevision
                    : EGamePlatformInteractionError::TargetUnavailable;
        return false;
    }

    bool bCommitted = false;
    switch (Option.CommitKind)
    {
    case EGamePlatformInteractionCommitKind::Toggle:
        bToggleState = !bToggleState;
        bCommitted = true;
        break;

    case EGamePlatformInteractionCommitKind::Consume:
        if (!bConsumed)
        {
            bConsumed = true;
            bEnabled = false;
            bCommitted = true;
        }
        break;

    case EGamePlatformInteractionCommitKind::Harvest:
        if (RemainingCharges > 0)
        {
            --RemainingCharges;
            if (RemainingCharges == 0)
            {
                bEnabled = false;
            }
            bCommitted = true;
        }
        break;

    case EGamePlatformInteractionCommitKind::External:
        // External提交只确认本次Interaction事实成立。
        // 目标最终Consumed状态必须由外部权威结果回调后显式Finalize。
        bCommitted = true;
        break;

    case EGamePlatformInteractionCommitKind::Custom:
    default:
        if (IGamePlatformInteractable* Interface =
            Cast<IGamePlatformInteractable>(GetOwner()))
        {
            bCommitted = Interface->CommitInteraction(Session, Option);
        }
        else
        {
            OutResult.Error =
                EGamePlatformInteractionError::OutcomeHandlerUnavailable;
            return false;
        }
        break;
    }

    if (!bCommitted)
    {
        OutResult.Error = EGamePlatformInteractionError::OutcomeUnknown;
        return false;
    }

    CommittedSessions.Add(Session.SessionId);
    CommittedSessionOrder.Add(Session.SessionId);
    const int32 MaxRememberedSessions =
        FMath::Max(
            8,
            GetDefault<UGamePlatformInteractionSettings>()
                ->MaxRecentRequests);

    while (CommittedSessionOrder.Num() >
        MaxRememberedSessions)
    {
        const FGuid Oldest =
            CommittedSessionOrder[0];
        CommittedSessionOrder.RemoveAt(
            0,
            1,
            EAllowShrinking::No);
        CommittedSessions.Remove(Oldest);
    }
    BumpRevision();

    OutResult.Error = EGamePlatformInteractionError::None;
    OutResult.State = EGamePlatformInteractionSessionState::Completed;
    OutResult.FinalTargetRevision = TargetRevision;
    return true;
}

bool UGamePlatformInteractableComponent::BeginExternalOutcomeReservation(
    const FGuid& ReservationId)
{
    if (!GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        bConsumed)
    {
        return false;
    }

    if (ExternalOutcomeReservationId.IsValid())
    {
        return ExternalOutcomeReservationId == ReservationId;
    }

    if (!bEnabled)
    {
        return false;
    }

    ExternalOutcomeReservationId = ReservationId;
    bEnabled = false;
    BumpRevision();
    CancelActiveSessions(
        EGamePlatformInteractionCancelReason::TargetUnavailable);
    return true;
}

bool UGamePlatformInteractableComponent::FinalizeExternalConsume(
    const FGuid& ReservationId)
{
    if (!GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        ExternalOutcomeReservationId != ReservationId ||
        bConsumed)
    {
        return false;
    }

    ExternalOutcomeReservationId.Invalidate();
    bConsumed = true;
    bEnabled = false;
    BumpRevision();
    return true;
}

bool UGamePlatformInteractableComponent::CancelExternalOutcomeReservation(
    const FGuid& ReservationId)
{
    if (!GetOwner()->HasAuthority() ||
        !ReservationId.IsValid() ||
        ExternalOutcomeReservationId != ReservationId ||
        bConsumed)
    {
        return false;
    }

    ExternalOutcomeReservationId.Invalidate();
    bEnabled = true;
    BumpRevision();
    return true;
}

void UGamePlatformInteractableComponent::OnRep_State()
{
    OnStateChanged.Broadcast();
}

void UGamePlatformInteractableComponent::CancelActiveSessions(
    EGamePlatformInteractionCancelReason Reason)
{
    if (!GetOwner()->HasAuthority() || ActiveSessions.IsEmpty())
    {
        return;
    }

    TArray<TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>> Sessions;
    Sessions.Reserve(ActiveSessions.Num());

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : ActiveSessions)
    {
        Sessions.Add(Pair);
    }

    for (const TPair<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>>& Pair : Sessions)
    {
        if (UGamePlatformInteractorComponent* Interactor = Pair.Value.Get())
        {
            Interactor->CancelFromTarget(Pair.Key, Reason);
        }
    }

    ActiveSessions.Reset();
    OccupancyCount = 0;
}

void UGamePlatformInteractableComponent::BumpRevision()
{
    ++TargetRevision;
    OnStateChanged.Broadcast();
}

bool UGamePlatformInteractableComponent::ValidateOptions(
    const TArray<FGamePlatformInteractionOption>& InOptions) const
{
    const UGamePlatformInteractionSettings* Settings =
        GetDefault<UGamePlatformInteractionSettings>();

    TSet<FName> OptionIds;
    for (const FGamePlatformInteractionOption& Option : InOptions)
    {
        if (!Option.IsStructurallyValid() ||
            Option.MaxDistance > Settings->MaxConfiguredInteractionDistance ||
            OptionIds.Contains(Option.OptionId))
        {
            return false;
        }

        OptionIds.Add(Option.OptionId);
    }

    return true;
}
