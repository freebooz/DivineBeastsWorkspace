// 平台成长投影事件实现；客户端没有XP权威，生命周期与派生缓存合同见同名公开头。
#include "Services/GamePlatformProgressionClientSubsystem.h"

#include "Definitions/GamePlatformProgressionTrackDefinition.h"
#include "Interfaces/GamePlatformProgressionClientTransport.h"

void UGamePlatformProgressionClientSubsystem::Deinitialize()
{
    OnViewChanged.Clear();
    OnLevelChanged.Clear();
    OnXPChanged.Clear();
    ResetAccount();
    Definitions.Reset();
    Super::Deinitialize();
}

bool UGamePlatformProgressionClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformProgressionClientTransport, ESPMode::ThreadSafe>
        InTransport)
{
    if (AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    ResetAccount();
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    return RefreshSnapshot();
}

void UGamePlatformProgressionClientSubsystem::ResetAccount()
{
    ++AccountGeneration;

    if (Transport.IsValid())
    {
        Transport->CancelAllRequests();
    }

    CurrentAccountKey.Reset();
    Transport.Reset();
    Snapshot = {};
    ++SnapshotGeneration;
    CachedTrackIndexById.Reset();
    CachedViewModels.Reset();
    CachedIndexSnapshotGeneration = ~uint64(0);
    CachedViewSnapshotGeneration = ~uint64(0);
    CachedViewDefinitionGeneration = ~uint64(0);
    State = EGamePlatformProgressionClientState::Uninitialized;
    LastError = EGamePlatformProgressionError::None;
    PublishViewChanged();
}

void UGamePlatformProgressionClientSubsystem::PublishViewChanged()
{
    check(IsInGameThread());
    ++ViewGeneration;
    OnViewChanged.Broadcast();
}

bool UGamePlatformProgressionClientSubsystem::RefreshSnapshot()
{
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        State == EGamePlatformProgressionClientState::Loading ||
        State == EGamePlatformProgressionClientState::Reconciling)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;

    State =
        Snapshot.ProgressionRevision > 0
            ? EGamePlatformProgressionClientState::Reconciling
            : EGamePlatformProgressionClientState::Loading;

    LastError = EGamePlatformProgressionError::None;

    TWeakObjectPtr<UGamePlatformProgressionClientSubsystem> WeakThis(this);

    const bool bStarted =
        Transport->BeginGetSnapshot(
            [WeakThis, ExpectedGeneration](
                FGamePlatformProgressionSnapshot NewSnapshot,
                EGamePlatformProgressionError Error)
            {
                if (UGamePlatformProgressionClientSubsystem* Self =
                    WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted)
    {
        State = EGamePlatformProgressionClientState::Error;
        LastError = EGamePlatformProgressionError::BackendUnavailable;
    }

    // 受理/启动失败也必须通知忙碌与错误；账号若在同步完成中改变，保留新账号状态。
    if (AccountGeneration == ExpectedGeneration) { PublishViewChanged(); }
    return bStarted;
}

void UGamePlatformProgressionClientSubsystem::RegisterTrackDefinition(
    UGamePlatformProgressionTrackDefinition* Definition)
{
    if (!IsValid(Definition) ||
        Definition->ProgressionTrackId.IsNone())
    {
        return;
    }

    FString Reason;
    if (!Definition->Validate(Reason))
    {
        return;
    }

    UGamePlatformProgressionTrackDefinition* Existing =
        Definitions.FindRef(Definition->ProgressionTrackId);
    if (Existing == Definition)
    {
        return;
    }

    Definitions.Add(
        Definition->ProgressionTrackId,
        Definition);
    ++DefinitionGeneration;
    PublishViewChanged();
}

int32 UGamePlatformProgressionClientSubsystem::GetLevel(
    FName TrackId,
    const FString& SubjectId) const
{
    const FGamePlatformProgressionTrackState* Track =
        FindTrack(TrackId, SubjectId);

    return Track ? Track->Level : 0;
}

int64 UGamePlatformProgressionClientSubsystem::GetTotalXP(
    FName TrackId,
    const FString& SubjectId) const
{
    const FGamePlatformProgressionTrackState* Track =
        FindTrack(TrackId, SubjectId);

    return Track ? Track->TotalXP : 0;
}

void UGamePlatformProgressionClientSubsystem::EnsureTrackIndexCache() const
{
    if (CachedIndexSnapshotGeneration == SnapshotGeneration)
    {
        return;
    }

    CachedTrackIndexById.Reset();
    for (int32 Index = 0; Index < Snapshot.Tracks.Num(); ++Index)
    {
        const FGamePlatformProgressionTrackState& Track = Snapshot.Tracks[Index];
        if (!Track.ProgressionTrackId.IsNone() && !Track.SubjectId.IsEmpty())
        {
            CachedTrackIndexById.FindOrAdd(Track.ProgressionTrackId)
                .Add(Track.SubjectId, Index);
        }
    }
    CachedIndexSnapshotGeneration = SnapshotGeneration;
}

void UGamePlatformProgressionClientSubsystem::EnsureViewModelCache() const
{
    if (CachedViewSnapshotGeneration == SnapshotGeneration &&
        CachedViewDefinitionGeneration == DefinitionGeneration)
    {
        return;
    }

    CachedViewModels.Reset();
    CachedViewModels.Reserve(Snapshot.Tracks.Num());
    for (const FGamePlatformProgressionTrackState& Track : Snapshot.Tracks)
    {
        FGamePlatformProgressionViewModel View;
        View.ProgressionTrackId = Track.ProgressionTrackId;
        View.SubjectId = Track.SubjectId;
        View.Level = Track.Level;
        View.MaxLevel = Track.MaxLevel;
        View.TotalXP = Track.TotalXP;

        UGamePlatformProgressionTrackDefinition* Definition =
            Definitions.FindRef(Track.ProgressionTrackId);
        if (IsValid(Definition) &&
            Definition->CurveVersion == Track.CurveVersion &&
            Definition->SubjectType == Track.SubjectType)
        {
            View.bCurveCompatible = true;
            View.XPIntoLevel = Definition->CalculateXPIntoLevel(Track.TotalXP);
            View.XPForNextLevel = Definition->CalculateXPForNextLevel(Track.TotalXP);
            if (Track.Level >= Track.MaxLevel)
            {
                View.ProgressPercent = 1.0f;
            }
            else if (View.XPForNextLevel > 0)
            {
                View.ProgressPercent = FMath::Clamp(
                    static_cast<float>(View.XPIntoLevel) /
                        static_cast<float>(View.XPForNextLevel),
                    0.0f,
                    1.0f);
            }
        }
        CachedViewModels.Add(MoveTemp(View));
    }

    CachedViewSnapshotGeneration = SnapshotGeneration;
    CachedViewDefinitionGeneration = DefinitionGeneration;
}

const TArray<FGamePlatformProgressionViewModel>&
UGamePlatformProgressionClientSubsystem::GetViewModelsView() const
{
    EnsureViewModelCache();
    return CachedViewModels;
}

TArray<FGamePlatformProgressionViewModel>
UGamePlatformProgressionClientSubsystem::GetViewModels() const
{
    return GetViewModelsView();
}

void UGamePlatformProgressionClientSubsystem::HandleSnapshotCompleted(
    uint64 ExpectedGeneration,
    FGamePlatformProgressionSnapshot NewSnapshot,
    EGamePlatformProgressionError Error)
{
    if (ExpectedGeneration != AccountGeneration)
    {
        return;
    }

    if (Error != EGamePlatformProgressionError::None)
    {
        State = EGamePlatformProgressionClientState::Error;
        LastError = Error;
        PublishViewChanged();
        return;
    }

    if (!ApplySnapshot(NewSnapshot))
    {
        State = EGamePlatformProgressionClientState::Error;
        LastError = EGamePlatformProgressionError::InvalidResponse;
        PublishViewChanged();
        return;
    }

    if (ExpectedGeneration != AccountGeneration) { return; }
    State = EGamePlatformProgressionClientState::Ready;
    LastError = EGamePlatformProgressionError::None;
    PublishViewChanged();
}

bool UGamePlatformProgressionClientSubsystem::ApplySnapshot(
    const FGamePlatformProgressionSnapshot& NewSnapshot)
{
    if (!NewSnapshot.IsValid())
    {
        return false;
    }

    if (Snapshot.ProgressionRevision > 0 &&
        NewSnapshot.ProgressionRevision <
            Snapshot.ProgressionRevision)
    {
        return false;
    }

    TMap<FString, FGamePlatformProgressionTrackState> Previous;
    for (const FGamePlatformProgressionTrackState& Track :
         Snapshot.Tracks)
    {
        const FString Key =
            Track.ProgressionTrackId.ToString() +
            TEXT("|") +
            Track.SubjectId;
        Previous.Add(Key, Track);
    }

    Snapshot = NewSnapshot;
    ++SnapshotGeneration;

    // XP/等级兼容事件可能重入ResetAccount；遍历不可变副本并在每次广播后核对账号代次。
    const auto AcceptedTracks = Snapshot.Tracks;
    const uint64 ExpectedAccountGeneration = AccountGeneration;
    for (const FGamePlatformProgressionTrackState& Track : AcceptedTracks)
    {
        const FString Key = Track.ProgressionTrackId.ToString() + TEXT("|") + Track.SubjectId;
        const FGamePlatformProgressionTrackState* Old =
            Previous.Find(Key);

        if (!Old)
        {
            continue;
        }

        if (Track.TotalXP != Old->TotalXP)
        {
            OnXPChanged.Broadcast(
                Track.ProgressionTrackId,
                Track.SubjectId,
                Old->TotalXP,
                Track.TotalXP);
            if (ExpectedAccountGeneration != AccountGeneration) { return true; }
        }

        if (Track.Level > Old->Level)
        {
            OnLevelChanged.Broadcast(
                Track.ProgressionTrackId,
                Track.SubjectId,
                Old->Level,
                Track.Level);
            if (ExpectedAccountGeneration != AccountGeneration) { return true; }
        }
    }

    return true;
}

const FGamePlatformProgressionTrackState*
UGamePlatformProgressionClientSubsystem::FindTrack(
    FName TrackId,
    const FString& SubjectId) const
{
    if (TrackId.IsNone() || SubjectId.IsEmpty())
    {
        return nullptr;
    }

    EnsureTrackIndexCache();
    const TMap<FString, int32>* SubjectIndex =
        CachedTrackIndexById.Find(TrackId);
    if (!SubjectIndex)
    {
        return nullptr;
    }

    const int32* Index = SubjectIndex->Find(SubjectId);
    return Index && Snapshot.Tracks.IsValidIndex(*Index)
        ? &Snapshot.Tracks[*Index]
        : nullptr;
}
