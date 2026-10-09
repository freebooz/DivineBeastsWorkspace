// 平台运营投影事件实现；服务器权威与本地时间派生视图的代次分离。
#include "Services/GamePlatformLiveOpsClientSubsystem.h"

#include "Interfaces/GamePlatformLiveOpsClientTransport.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"

void UGamePlatformLiveOpsClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ForegroundHandle =
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
            this,
            &UGamePlatformLiveOpsClientSubsystem::HandleEnteredForeground);

    BoundaryTickerHandle =
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UGamePlatformLiveOpsClientSubsystem::TickBoundaryRefresh),
            1.0f);
}

void UGamePlatformLiveOpsClientSubsystem::Deinitialize()
{
    if (ForegroundHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(
            ForegroundHandle);
        ForegroundHandle.Reset();
    }

    if (BoundaryTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(BoundaryTickerHandle);
        BoundaryTickerHandle.Reset();
    }

    OnViewChanged.Clear();
    OnCatalogChanged.Clear();
    OnPlayerStateChanged.Clear();
    OnClaimChanged.Clear();
    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    ++AccountGeneration;
    CurrentAccountKey.Reset();
    Transport.Reset();
    Catalog = {};
    PlayerState = {};
    LastClaim = {};
    ServerTimeEstimator.Reset();
    State = EGamePlatformLiveOpsClientState::Uninitialized;
    LastError = EGamePlatformLiveOpsError::None;
    bCatalogRequestInFlight = false;
    bPlayerStateRequestInFlight = false;
    bClaimRequestInFlight = false;
    bClaimReconcileInFlight = false;
    ActiveClaimOperationId.Invalidate();

    Super::Deinitialize();
}

bool UGamePlatformLiveOpsClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformLiveOpsClientTransport, ESPMode::ThreadSafe>
        InTransport)
{
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);

    return RefreshAll();
}

void UGamePlatformLiveOpsClientSubsystem::ResetAccount()
{
    ++AccountGeneration;

    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Transport.Reset();

    // Catalog（运营目录）是全局Published数据，可在同一个LocalPlayer作用域保留。
    // PlayerState（玩家状态）必须随账号切换清空。
    PlayerState = {};
    LastClaim = {};

    bCatalogRequestInFlight = false;
    bPlayerStateRequestInFlight = false;
    bClaimRequestInFlight = false;
    bClaimReconcileInFlight = false;
    ActiveClaimOperationId.Invalidate();

    LastError = EGamePlatformLiveOpsError::None;
    LastBoundaryCheckUtc = GetEstimatedServerNowUtc();

    State =
        Catalog.CatalogRevision > 0
            ? EGamePlatformLiveOpsClientState::Ready
            : EGamePlatformLiveOpsClientState::Uninitialized;
    PublishDerivedViewChanged(false, true);
}

bool UGamePlatformLiveOpsClientSubsystem::RefreshAll()
{
    const bool bCatalog = RefreshCatalog();
    const bool bPlayer = RefreshPlayerState();
    return bCatalog || bPlayer;
}

bool UGamePlatformLiveOpsClientSubsystem::RefreshCatalog()
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        bCatalogRequestInFlight)
    {
        return false;
    }

    bCatalogRequestInFlight = true;
    State =
        Catalog.CatalogRevision > 0
            ? EGamePlatformLiveOpsClientState::Reconciling
            : EGamePlatformLiveOpsClientState::Loading;

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformLiveOpsClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetCatalog(
            [WeakThis, ExpectedGeneration](
                FGamePlatformLiveOpsCatalogSnapshot NewCatalog,
                EGamePlatformLiveOpsError Error)
            {
                if (UGamePlatformLiveOpsClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleCatalogCompleted(
                        ExpectedGeneration,
                        MoveTemp(NewCatalog),
                        Error);
                }
            });

    if (!bStarted)
    {
        bCatalogRequestInFlight = false;
        State = EGamePlatformLiveOpsClientState::Error;
        LastError = EGamePlatformLiveOpsError::BackendUnavailable;
    }

    if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(false, false); }
    return bStarted;
}

bool UGamePlatformLiveOpsClientSubsystem::RefreshPlayerState()
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        bPlayerStateRequestInFlight)
    {
        return false;
    }

    bPlayerStateRequestInFlight = true;
    State =
        PlayerState.PlayerStateRevision > 0
            ? EGamePlatformLiveOpsClientState::Reconciling
            : EGamePlatformLiveOpsClientState::Loading;

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformLiveOpsClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetPlayerState(
            [WeakThis, ExpectedGeneration](
                FGamePlatformLiveOpsPlayerState NewState,
                EGamePlatformLiveOpsError Error)
            {
                if (UGamePlatformLiveOpsClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandlePlayerStateCompleted(
                        ExpectedGeneration,
                        MoveTemp(NewState),
                        Error);
                }
            });

    if (!bStarted)
    {
        bPlayerStateRequestInFlight = false;
        State = EGamePlatformLiveOpsClientState::Error;
        LastError = EGamePlatformLiveOpsError::BackendUnavailable;
    }

    if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(false, false); }
    return bStarted;
}

bool UGamePlatformLiveOpsClientSubsystem::ClaimSignIn(
    FName CampaignId,
    const FGuid& ClaimOperationId)
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        CampaignId.IsNone() ||
        !ClaimOperationId.IsValid() ||
        bClaimRequestInFlight ||
        bClaimReconcileInFlight)
    {
        return false;
    }

    bClaimRequestInFlight = true;
    ActiveClaimOperationId = ClaimOperationId;
    State = EGamePlatformLiveOpsClientState::Reconciling;

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformLiveOpsClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginClaimSignIn(
            ClaimOperationId,
            CampaignId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformLiveOpsClaimResult Result,
                EGamePlatformLiveOpsError Error)
            {
                if (UGamePlatformLiveOpsClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleClaimCompleted(
                        ExpectedGeneration,
                        MoveTemp(Result),
                        Error);
                }
            });

    if (!bStarted)
    {
        bClaimRequestInFlight = false;
        ActiveClaimOperationId.Invalidate();
        State = EGamePlatformLiveOpsClientState::Error;
        LastError = EGamePlatformLiveOpsError::BackendUnavailable;
    }

    if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(false, false); }
    return bStarted;
}

bool UGamePlatformLiveOpsClientSubsystem::ReconcileClaim(
    const FGuid& ClaimOperationId)
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        !ClaimOperationId.IsValid() ||
        bClaimRequestInFlight ||
        bClaimReconcileInFlight)
    {
        return false;
    }

    bClaimReconcileInFlight = true;
    ActiveClaimOperationId = ClaimOperationId;
    State = EGamePlatformLiveOpsClientState::Reconciling;

    const uint64 ExpectedGeneration = AccountGeneration;
    TWeakObjectPtr<UGamePlatformLiveOpsClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginQueryClaimOperation(
            ClaimOperationId,
            [WeakThis, ExpectedGeneration](
                FGamePlatformLiveOpsClaimResult Result,
                EGamePlatformLiveOpsError Error)
            {
                if (UGamePlatformLiveOpsClientSubsystem* Self =
                        WeakThis.Get())
                {
                    Self->HandleClaimCompleted(
                        ExpectedGeneration,
                        MoveTemp(Result),
                        Error);
                }
            });

    if (!bStarted)
    {
        bClaimReconcileInFlight = false;
        ActiveClaimOperationId.Invalidate();
        State = EGamePlatformLiveOpsClientState::Error;
        LastError = EGamePlatformLiveOpsError::BackendUnavailable;
    }

    if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(false, false); }
    return bStarted;
}

TArray<FName>
UGamePlatformLiveOpsClientSubsystem::GetActiveSeasonIds() const
{
    struct FActiveSeason
    {
        FName Id;
        int32 Priority = 0;
    };

    TArray<FActiveSeason> Active;
    const FDateTime Now = GetEstimatedServerNowUtc();
    if (Now.GetTicks() <= 0)
    {
        return {};
    }

    for (const FGamePlatformLiveOpsSeason& Season : Catalog.Seasons)
    {
        if (Season.TimeWindow.IsActive(Now))
        {
            Active.Add({Season.SeasonId, Season.Priority});
        }
    }

    Active.Sort(
        [](const FActiveSeason& A, const FActiveSeason& B)
        {
            if (A.Priority != B.Priority)
            {
                return A.Priority > B.Priority;
            }
            return A.Id.LexicalLess(B.Id);
        });

    TArray<FName> Result;
    Result.Reserve(Active.Num());
    for (const FActiveSeason& Item : Active)
    {
        Result.Add(Item.Id);
    }
    return Result;
}

TArray<FGamePlatformLiveOpsEventViewModel>
UGamePlatformLiveOpsClientSubsystem::GetEventViewModels() const
{
    TArray<FGamePlatformLiveOpsEventViewModel> Result;
    const FDateTime Now = GetEstimatedServerNowUtc();
    if (Now.GetTicks() <= 0)
    {
        return Result;
    }

    Result.Reserve(Catalog.Events.Num());

    for (const FGamePlatformLiveOpsEvent& Event : Catalog.Events)
    {
        FGamePlatformLiveOpsEventViewModel View;
        View.EventId = Event.EventId;
        View.bActive = Event.TimeWindow.IsActive(Now);
        View.PresentationMetadataId = Event.PresentationMetadataId;

        if (Now < Event.TimeWindow.StartsAtUtc)
        {
            View.TimeUntilStartSeconds =
                (Event.TimeWindow.StartsAtUtc - Now).GetTotalSeconds();
        }

        if (Event.TimeWindow.bHasEnd &&
            Now < Event.TimeWindow.EndsAtUtc)
        {
            View.TimeUntilEndSeconds =
                (Event.TimeWindow.EndsAtUtc - Now).GetTotalSeconds();
        }

        Result.Add(MoveTemp(View));
    }

    return Result;
}

TArray<FGamePlatformLiveOpsSignInViewModel>
UGamePlatformLiveOpsClientSubsystem::GetSignInViewModels() const
{
    TArray<FGamePlatformLiveOpsSignInViewModel> Result;
    const FDateTime Now = GetEstimatedServerNowUtc();
    if (Now.GetTicks() <= 0)
    {
        return Result;
    }

    Result.Reserve(Catalog.SignInCampaigns.Num());

    for (const FGamePlatformLiveOpsSignInCampaign& Campaign :
         Catalog.SignInCampaigns)
    {
        FGamePlatformLiveOpsSignInViewModel View;
        View.CampaignId = Campaign.CampaignId;
        View.bActive = Campaign.TimeWindow.IsActive(Now);

        if (Now < Campaign.TimeWindow.StartsAtUtc)
        {
            View.TimeUntilStartSeconds =
                (Campaign.TimeWindow.StartsAtUtc - Now).GetTotalSeconds();
        }

        if (Campaign.TimeWindow.bHasEnd &&
            Now < Campaign.TimeWindow.EndsAtUtc)
        {
            View.TimeUntilEndSeconds =
                (Campaign.TimeWindow.EndsAtUtc - Now).GetTotalSeconds();
        }

        const FGamePlatformLiveOpsCampaignState* StateEntry =
            PlayerState.CampaignStates.FindByPredicate(
                [&Campaign](
                    const FGamePlatformLiveOpsCampaignState& Entry)
                {
                    return Entry.CampaignId == Campaign.CampaignId;
                });

        if (StateEntry)
        {
            View.bClaimedCurrentPeriod =
                StateEntry->bClaimedCurrentPeriod;
            View.TotalClaimCount =
                StateEntry->TotalClaimCount;
            View.NextRewardIndex =
                StateEntry->NextRewardIndex;
            View.RewardStatus =
                StateEntry->RewardStatus;

            View.bClaimable =
                View.bActive &&
                !View.bClaimedCurrentPeriod &&
                StateEntry->RewardStatus == TEXT("available");
        }

        Result.Add(MoveTemp(View));
    }

    return Result;
}


bool UGamePlatformLiveOpsClientSubsystem::TickBoundaryRefresh(
    float)
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        !ServerTimeEstimator.IsValid() ||
        Catalog.CatalogRevision <= 0)
    {
        return true;
    }

    const FDateTime Now = GetEstimatedServerNowUtc();
    if (Now.GetTicks() <= 0)
    {
        return true;
    }

    if (LastBoundaryCheckUtc.GetTicks() <= 0 ||
        Now <= LastBoundaryCheckUtc)
    {
        LastBoundaryCheckUtc = Now;
        return true;
    }

    const bool bCrossed =
        HasCrossedCatalogBoundary(
            LastBoundaryCheckUtc,
            Now);

    LastBoundaryCheckUtc = Now;

    if (bCrossed)
    {
        // 时间有效性先发生变化，网络刷新可能失败/Revision不变，仍需立即告知本地派生视图。
        const uint64 ExpectedGeneration = AccountGeneration;
        PublishDerivedViewChanged(true, true);
        if (ExpectedGeneration != AccountGeneration) { return true; }
        RefreshAll();
    }

    return true;
}

bool UGamePlatformLiveOpsClientSubsystem::HasCrossedCatalogBoundary(
    const FDateTime& PreviousUtc,
    const FDateTime& CurrentUtc) const
{
    const auto Crossed =
        [&PreviousUtc, &CurrentUtc](const FDateTime& Boundary)
        {
            return Boundary.GetTicks() > 0 &&
                   PreviousUtc < Boundary &&
                   CurrentUtc >= Boundary;
        };

    for (const FGamePlatformLiveOpsSeason& Season : Catalog.Seasons)
    {
        if (Crossed(Season.TimeWindow.StartsAtUtc) ||
            (Season.TimeWindow.bHasEnd &&
             Crossed(Season.TimeWindow.EndsAtUtc)))
        {
            return true;
        }
    }

    for (const FGamePlatformLiveOpsEvent& Event : Catalog.Events)
    {
        if (Crossed(Event.TimeWindow.StartsAtUtc) ||
            (Event.TimeWindow.bHasEnd &&
             Crossed(Event.TimeWindow.EndsAtUtc)))
        {
            return true;
        }
    }

    for (const FGamePlatformLiveOpsSignInCampaign& Campaign :
         Catalog.SignInCampaigns)
    {
        if (Crossed(Campaign.TimeWindow.StartsAtUtc) ||
            (Campaign.TimeWindow.bHasEnd &&
             Crossed(Campaign.TimeWindow.EndsAtUtc)))
        {
            return true;
        }
    }

    return false;
}

void UGamePlatformLiveOpsClientSubsystem::HandleEnteredForeground()
{
    if (!CurrentAccountKey.IsEmpty() &&
        Transport.IsValid())
    {
        RefreshAll();
    }
}

void UGamePlatformLiveOpsClientSubsystem::HandleCatalogCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformLiveOpsCatalogSnapshot NewCatalog,
    EGamePlatformLiveOpsError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    bCatalogRequestInFlight = false;

    if (Error != EGamePlatformLiveOpsError::None)
    {
        LastError = Error;
        RefreshAggregateState();
        return;
    }

    if (NewCatalog.CatalogRevision < 1 ||
        NewCatalog.ServerTimeUtc.GetTicks() <= 0)
    {
        LastError = EGamePlatformLiveOpsError::InvalidResponse;
        RefreshAggregateState();
        return;
    }

    ServerTimeEstimator.Update(NewCatalog.ServerTimeUtc);
    LastBoundaryCheckUtc =
        ServerTimeEstimator.EstimatedServerNowUtc();

    if (NewCatalog.CatalogRevision >= Catalog.CatalogRevision)
    {
        Catalog = MoveTemp(NewCatalog);
        // 服务器时间采样和派生有效性可变化，而目录持久Revision不推进。
        LastError = EGamePlatformLiveOpsError::None;
        RefreshAggregateState();
        if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(true, true); }
        return;
    }

    LastError = EGamePlatformLiveOpsError::None;
    RefreshAggregateState();
}

void UGamePlatformLiveOpsClientSubsystem::HandlePlayerStateCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformLiveOpsPlayerState NewState,
    EGamePlatformLiveOpsError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    bPlayerStateRequestInFlight = false;

    if (Error != EGamePlatformLiveOpsError::None)
    {
        LastError = Error;
        RefreshAggregateState();
        return;
    }

    if (NewState.PlayerStateRevision < 1 ||
        NewState.ServerTimeUtc.GetTicks() <= 0)
    {
        LastError = EGamePlatformLiveOpsError::InvalidResponse;
        RefreshAggregateState();
        return;
    }

    ServerTimeEstimator.Update(NewState.ServerTimeUtc);

    if (NewState.PlayerStateRevision >=
        PlayerState.PlayerStateRevision)
    {
        PlayerState = MoveTemp(NewState);
        LastError = EGamePlatformLiveOpsError::None;
        RefreshAggregateState();
        if (ExpectedGeneration == AccountGeneration) { PublishDerivedViewChanged(false, true); }
        return;
    }

    LastError = EGamePlatformLiveOpsError::None;
    RefreshAggregateState();
}

void UGamePlatformLiveOpsClientSubsystem::HandleClaimCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformLiveOpsClaimResult Result,
    EGamePlatformLiveOpsError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    const bool bWasReconcile = bClaimReconcileInFlight;
    bClaimRequestInFlight = false;
    bClaimReconcileInFlight = false;

    if (Error != EGamePlatformLiveOpsError::None)
    {
        LastError = Error;

        const bool bNeedsReconcile =
            !bWasReconcile &&
            ActiveClaimOperationId.IsValid() &&
            (Error == EGamePlatformLiveOpsError::AlreadyClaimed ||
             Error == EGamePlatformLiveOpsError::ClaimInProgress ||
             Error == EGamePlatformLiveOpsError::OutcomeUnknown ||
             Error == EGamePlatformLiveOpsError::RewardGrantFailed ||
             Error == EGamePlatformLiveOpsError::BackendUnavailable);

        if (bNeedsReconcile)
        {
            const FGuid OperationId = ActiveClaimOperationId;
            ActiveClaimOperationId.Invalidate();
            ReconcileClaim(OperationId);
            return;
        }

        ActiveClaimOperationId.Invalidate();
        RefreshAggregateState();
        return;
    }

    if (Result.ClaimOperationId.IsEmpty() ||
        Result.PlayerStateRevision < 1 ||
        Result.ServerTimeUtc.GetTicks() <= 0)
    {
        LastError = EGamePlatformLiveOpsError::InvalidResponse;
        ActiveClaimOperationId.Invalidate();
        RefreshAggregateState();
        return;
    }

    LastClaim = Result;
    ActiveClaimOperationId.Invalidate();
    ServerTimeEstimator.Update(Result.ServerTimeUtc);
    OnClaimChanged.Broadcast(Result);
    if (ExpectedGeneration != AccountGeneration) { return; }

    // Claim成功或查到持久结果后刷新PlayerState，
    // 不由客户端自行修改ClaimedCurrentPeriod。
    RefreshPlayerState();
}

void UGamePlatformLiveOpsClientSubsystem::RefreshAggregateState()
{
    if (bCatalogRequestInFlight ||
        bPlayerStateRequestInFlight ||
        bClaimRequestInFlight ||
        bClaimReconcileInFlight)
    {
        State =
            (Catalog.CatalogRevision > 0 ||
             PlayerState.PlayerStateRevision > 0)
                ? EGamePlatformLiveOpsClientState::Reconciling
                : EGamePlatformLiveOpsClientState::Loading;
        PublishDerivedViewChanged(false, false);
        return;
    }

    if (LastError != EGamePlatformLiveOpsError::None &&
        Catalog.CatalogRevision <= 0 &&
        PlayerState.PlayerStateRevision <= 0)
    {
        State = EGamePlatformLiveOpsClientState::Error;
        PublishDerivedViewChanged(false, false);
        return;
    }

    if (Catalog.CatalogRevision > 0 &&
        PlayerState.PlayerStateRevision > 0 &&
        ServerTimeEstimator.IsValid())
    {
        State = EGamePlatformLiveOpsClientState::Ready;
        PublishDerivedViewChanged(false, false);
        return;
    }

    State = EGamePlatformLiveOpsClientState::Uninitialized;
    PublishDerivedViewChanged(false, false);
}

void UGamePlatformLiveOpsClientSubsystem::PublishDerivedViewChanged(bool bCatalog, bool bPlayer)
{
    check(IsInGameThread());
    const uint64 ExpectedGeneration = AccountGeneration;
    ++ViewGeneration;
    OnViewChanged.Broadcast();
    if (ExpectedGeneration != AccountGeneration) { return; }
    if (bCatalog) { OnCatalogChanged.Broadcast(); }
    if (ExpectedGeneration != AccountGeneration) { return; }
    if (bPlayer) { OnPlayerStateChanged.Broadcast(); }
}
