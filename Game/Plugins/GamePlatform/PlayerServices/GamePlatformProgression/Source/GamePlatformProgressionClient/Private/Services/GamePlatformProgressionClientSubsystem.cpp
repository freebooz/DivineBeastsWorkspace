// 平台成长投影事件实现；客户端没有XP权威，生命周期与派生缓存合同见同名公开头。
#include "Services/GamePlatformProgressionClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Transport/GamePlatformProgressionGatewayHttpTransport.h"

#include "Definitions/GamePlatformProgressionTrackDefinition.h"
#include "Interfaces/GamePlatformProgressionClientTransport.h"

void UGamePlatformProgressionClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // Initialize建立新实例代次；退出后的迟到回调不能跨重新初始化消费。
    bDeinitializing = false; ++InstanceGeneration; ++AccountGeneration;
    Super::Initialize(Collection);
    BindOnlineAuthentication();
}

void UGamePlatformProgressionClientSubsystem::Deinitialize()
{
    // 先关闭作用域，再执行任何取消或广播；外部回调不得恢复账号。
    bDeinitializing = true; ++InstanceGeneration; ++AccountGeneration;
    UnbindOnlineAuthentication();
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
    check(IsInGameThread());
    if (bDeinitializing || bResettingAccount || AccountKey.IsEmpty() || !InTransport.IsValid())
    {
        return false;
    }

    const uint64 ExpectedInstanceGeneration = InstanceGeneration;
    ResetAccount();
    if (bDeinitializing || ExpectedInstanceGeneration != InstanceGeneration) { return false; }
    CurrentAccountKey = AccountKey;
    Transport = MoveTemp(InTransport);
    return RefreshSnapshot();
}

void UGamePlatformProgressionClientSubsystem::ResetAccount()
{
    check(IsInGameThread());
    if (bResettingAccount) { return; }
    TGuardValue<bool> ResetGuard(bResettingAccount, true);
    ++AccountGeneration;
    ++SnapshotRequestGeneration;

    const auto CancelledTransport = Transport;
    if (CancelledTransport.IsValid())
    {
        CancelledTransport->CancelAllRequests();
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
    if (bDeinitializing) { return; }
    check(IsInGameThread());
    ++ViewGeneration;
    OnViewChanged.Broadcast();
}

bool UGamePlatformProgressionClientSubsystem::RefreshSnapshot()
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        State == EGamePlatformProgressionClientState::Loading ||
        State == EGamePlatformProgressionClientState::Reconciling)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const uint64 ExpectedSnapshotRequestGeneration = ++SnapshotRequestGeneration;
    const auto RequestTransport = Transport;

    State =
        Snapshot.ProgressionRevision > 0
            ? EGamePlatformProgressionClientState::Reconciling
            : EGamePlatformProgressionClientState::Loading;

    LastError = EGamePlatformProgressionError::None;

    TWeakObjectPtr<UGamePlatformProgressionClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetSnapshot(
            [WeakThis, ExpectedGeneration, ExpectedSnapshotRequestGeneration](
                FGamePlatformProgressionSnapshot NewSnapshot,
                EGamePlatformProgressionError Error)
            {
                if (UGamePlatformProgressionClientSubsystem* Self =
                    WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        ExpectedSnapshotRequestGeneration,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && ExpectedSnapshotRequestGeneration == SnapshotRequestGeneration && RequestTransport == Transport && (State == EGamePlatformProgressionClientState::Loading || State == EGamePlatformProgressionClientState::Reconciling))
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
    if (bDeinitializing) { return; }
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
    uint64 ExpectedSnapshotRequestGeneration,
    FGamePlatformProgressionSnapshot NewSnapshot,
    EGamePlatformProgressionError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedSnapshotRequestGeneration != SnapshotRequestGeneration ||
        (State != EGamePlatformProgressionClientState::Loading && State != EGamePlatformProgressionClientState::Reconciling))
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

// Online负责认证/刷新/请求签名；领域层只消费脱敏认证快照，缺失路由仍走真实失败终态。
void UGamePlatformProgressionClientSubsystem::BindOnlineAuthentication()
{
    if (bDeinitializing) { return; }
    auto* LocalPlayer = GetLocalPlayer();
    auto* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    if (!IsValid(Online)) { return; }
    OnlineSubsystem = Online;
    if (!AuthStateChangedHandle.IsValid())
    { AuthStateChangedHandle = Online->OnAuthStateChanged().AddUObject(this, &UGamePlatformProgressionClientSubsystem::HandleAuthStateChanged); }
    HandleAuthStateChanged(Online->GetSnapshot());
}

void UGamePlatformProgressionClientSubsystem::UnbindOnlineAuthentication()
{
    if (auto* Online = OnlineSubsystem.Get())
    { if (AuthStateChangedHandle.IsValid()) { Online->OnAuthStateChanged().Remove(AuthStateChangedHandle); } }
    AuthStateChangedHandle.Reset(); OnlineSubsystem.Reset();
}

void UGamePlatformProgressionClientSubsystem::HandleAuthStateChanged(const FGamePlatformAuthSnapshot& AuthSnapshot)
{
    if (bDeinitializing) { return; }
    check(IsInGameThread());
    if (AuthSnapshot.State == EGamePlatformAuthState::Refreshing) { return; }
    if (AuthSnapshot.State == EGamePlatformAuthState::Authenticated)
    {
        auto* Online = OnlineSubsystem.Get();
        if (!Online || AuthSnapshot.AccountId.IsEmpty()) { ResetAccount(); return; }
        if (CurrentAccountKey == AuthSnapshot.AccountId && Transport.IsValid()) { return; }
        ConfigureAuthenticatedAccount(AuthSnapshot.AccountId,
            MakeShared<FGamePlatformProgressionGatewayHttpTransport, ESPMode::ThreadSafe>(Online));
        return;
    }
    if (!CurrentAccountKey.IsEmpty() || Transport.IsValid()) { ResetAccount(); }
}
