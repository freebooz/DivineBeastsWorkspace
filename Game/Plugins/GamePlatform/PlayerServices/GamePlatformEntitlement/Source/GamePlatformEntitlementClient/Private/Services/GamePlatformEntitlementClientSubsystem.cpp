// 平台本地玩家权益投影实现：游戏线程事件与异步领域Transport，Online拥有认证；账号重置清空本地缓存并失效本领域回调。
#include "Services/GamePlatformEntitlementClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Transport/GamePlatformEntitlementGatewayHttpTransport.h"

#include "Interfaces/GamePlatformEntitlementClientTransport.h"
#include "Queries/GamePlatformEntitlementQuery.h"

void UGamePlatformEntitlementClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // Initialize建立新实例代次；退出后的迟到回调不能跨重新初始化消费。
    bDeinitializing = false; ++InstanceGeneration; ++AccountGeneration;
    Super::Initialize(Collection);
    BindOnlineAuthentication();
}

void UGamePlatformEntitlementClientSubsystem::Deinitialize()
{
    // 先关闭作用域，再执行任何取消或广播；外部回调不得恢复账号。
    bDeinitializing = true; ++InstanceGeneration; ++AccountGeneration;
    UnbindOnlineAuthentication();
    OnChanged.Clear();
    ResetAccount();
    Super::Deinitialize();
}

bool UGamePlatformEntitlementClientSubsystem::ConfigureAuthenticatedAccount(
    const FString& AccountKey,
    TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe> InTransport)
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

void UGamePlatformEntitlementClientSubsystem::ResetAccount()
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
    EffectiveEntitlementIds.Reset();
    EffectiveHeroIds.Reset();
    EffectiveSkinIds.Reset();
    CachedViewModels.Reset();
    LastError = EGamePlatformEntitlementError::None;
    State = EGamePlatformEntitlementClientState::Uninitialized;
    OnChanged.Broadcast();
}

bool UGamePlatformEntitlementClientSubsystem::RefreshSnapshot()
{
    if (bDeinitializing) { return false; }
    if (CurrentAccountKey.IsEmpty() ||
        !Transport.IsValid() ||
        State == EGamePlatformEntitlementClientState::Loading ||
        State == EGamePlatformEntitlementClientState::Reconciling)
    {
        return false;
    }

    const uint64 ExpectedGeneration = AccountGeneration;
    const uint64 ExpectedSnapshotRequestGeneration = ++SnapshotRequestGeneration;
    const auto RequestTransport = Transport;
    State =
        Snapshot.Revision > 0
            ? EGamePlatformEntitlementClientState::Reconciling
            : EGamePlatformEntitlementClientState::Loading;
    LastError = EGamePlatformEntitlementError::None;
    OnChanged.Broadcast();

    // 公开状态事件允许消费者重置账号；广播返回后必须再次核对请求拥有者。
    if (ExpectedGeneration != AccountGeneration || RequestTransport != Transport) { return false; }
    TWeakObjectPtr<UGamePlatformEntitlementClientSubsystem> WeakThis(this);

    const bool bStarted =
        RequestTransport->BeginGetSnapshot(
            [WeakThis, ExpectedGeneration, ExpectedSnapshotRequestGeneration](
                FGamePlatformEntitlementSnapshot NewSnapshot,
                EGamePlatformEntitlementError Error)
            {
                if (UGamePlatformEntitlementClientSubsystem* Self =
                    WeakThis.Get())
                {
                    Self->HandleSnapshotCompleted(
                        ExpectedGeneration,
                        ExpectedSnapshotRequestGeneration,
                        MoveTemp(NewSnapshot),
                        Error);
                }
            });

    if (!bStarted && !bDeinitializing && ExpectedGeneration == AccountGeneration && ExpectedSnapshotRequestGeneration == SnapshotRequestGeneration && RequestTransport == Transport && (State == EGamePlatformEntitlementClientState::Loading || State == EGamePlatformEntitlementClientState::Reconciling))
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = EGamePlatformEntitlementError::BackendUnavailable;
        OnChanged.Broadcast();
    }

    return bStarted;
}

bool UGamePlatformEntitlementClientSubsystem::HasEntitlement(
    FName EntitlementId) const
{
    return !EntitlementId.IsNone() && EffectiveEntitlementIds.Contains(EntitlementId);
}

bool UGamePlatformEntitlementClientSubsystem::HasAny(
    const TArray<FName>& EntitlementIds) const
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (EffectiveEntitlementIds.Contains(EntitlementId))
        {
            return true;
        }
    }
    return false;
}

bool UGamePlatformEntitlementClientSubsystem::HasAll(
    const TArray<FName>& EntitlementIds) const
{
    for (const FName EntitlementId : EntitlementIds)
    {
        if (!EffectiveEntitlementIds.Contains(EntitlementId))
        {
            return false;
        }
    }
    return true;
}

TArray<FGamePlatformEntitlementViewModel>
UGamePlatformEntitlementClientSubsystem::GetViewModels() const
{
    return CachedViewModels;
}

bool UGamePlatformEntitlementClientSubsystem::IsHeroUnlocked(
    FName HeroDefinitionId) const
{
    return !HeroDefinitionId.IsNone() && EffectiveHeroIds.Contains(HeroDefinitionId);
}

bool UGamePlatformEntitlementClientSubsystem::IsSkinUnlocked(
    FName SkinDefinitionId) const
{
    return !SkinDefinitionId.IsNone() && EffectiveSkinIds.Contains(SkinDefinitionId);
}

void UGamePlatformEntitlementClientSubsystem::RebuildDerivedCaches()
{
    EffectiveEntitlementIds.Reset();
    EffectiveHeroIds.Reset();
    EffectiveSkinIds.Reset();
    CachedViewModels.Reset();

    EffectiveEntitlementIds.Reserve(Snapshot.Entitlements.Num());
    EffectiveHeroIds.Reserve(Snapshot.Entitlements.Num());
    EffectiveSkinIds.Reserve(Snapshot.Entitlements.Num());
    CachedViewModels.Reserve(Snapshot.Entitlements.Num());

    for (const FGamePlatformEntitlementEntry& Entry : Snapshot.Entitlements)
    {
        if (Entry.IsEffective())
        {
            EffectiveEntitlementIds.Add(Entry.EntitlementId);
            if (Entry.TargetType == EGamePlatformEntitlementTargetType::Hero && !Entry.TargetId.IsNone())
            {
                EffectiveHeroIds.Add(Entry.TargetId);
            }
            else if (Entry.TargetType == EGamePlatformEntitlementTargetType::Skin && !Entry.TargetId.IsNone())
            {
                EffectiveSkinIds.Add(Entry.TargetId);
            }
        }

        FGamePlatformEntitlementViewModel View;
        View.EntitlementId = Entry.EntitlementId;
        View.Category = Entry.Category;
        View.TargetType = Entry.TargetType;
        View.TargetId = Entry.TargetId;
        View.bUnlocked = Entry.IsEffective();
        View.bHasExpiresAt = Entry.bHasExpiresAt;
        View.ExpiresAtUtc = Entry.ExpiresAtUtc;
        CachedViewModels.Add(MoveTemp(View));
    }
}

void UGamePlatformEntitlementClientSubsystem::HandleSnapshotCompleted(
    uint64 ExpectedGeneration,
    uint64 ExpectedSnapshotRequestGeneration,
    FGamePlatformEntitlementSnapshot NewSnapshot,
    EGamePlatformEntitlementError Error)
{
    if (bDeinitializing) { return; }
    if (ExpectedGeneration != AccountGeneration || ExpectedSnapshotRequestGeneration != SnapshotRequestGeneration ||
        (State != EGamePlatformEntitlementClientState::Loading && State != EGamePlatformEntitlementClientState::Reconciling))
    {
        return;
    }

    if (Error != EGamePlatformEntitlementError::None)
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = Error;
        OnChanged.Broadcast();
        return;
    }

    if (!ApplySnapshot(NewSnapshot))
    {
        State = EGamePlatformEntitlementClientState::Error;
        LastError = EGamePlatformEntitlementError::InvalidResponse;
        OnChanged.Broadcast();
        return;
    }

    State = EGamePlatformEntitlementClientState::Ready;
    LastError = EGamePlatformEntitlementError::None;
    OnChanged.Broadcast();
}

bool UGamePlatformEntitlementClientSubsystem::ApplySnapshot(
    const FGamePlatformEntitlementSnapshot& NewSnapshot)
{
    if (!NewSnapshot.IsValid())
    {
        return false;
    }

    if (Snapshot.Revision > 0 &&
        NewSnapshot.Revision < Snapshot.Revision)
    {
        return false;
    }

    Snapshot = NewSnapshot;
    RebuildDerivedCaches();
    return true;
}

// Online负责认证/刷新/请求签名；领域层只消费脱敏认证快照，缺失路由仍走真实失败终态。
void UGamePlatformEntitlementClientSubsystem::BindOnlineAuthentication()
{
    if (bDeinitializing) { return; }
    auto* LocalPlayer = GetLocalPlayer();
    auto* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
    auto* Online = Instance ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    if (!IsValid(Online)) { return; }
    OnlineSubsystem = Online;
    if (!AuthStateChangedHandle.IsValid())
    { AuthStateChangedHandle = Online->OnAuthStateChanged().AddUObject(this, &UGamePlatformEntitlementClientSubsystem::HandleAuthStateChanged); }
    HandleAuthStateChanged(Online->GetSnapshot());
}

void UGamePlatformEntitlementClientSubsystem::UnbindOnlineAuthentication()
{
    if (auto* Online = OnlineSubsystem.Get())
    { if (AuthStateChangedHandle.IsValid()) { Online->OnAuthStateChanged().Remove(AuthStateChangedHandle); } }
    AuthStateChangedHandle.Reset(); OnlineSubsystem.Reset();
}

void UGamePlatformEntitlementClientSubsystem::HandleAuthStateChanged(const FGamePlatformAuthSnapshot& AuthSnapshot)
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
            MakeShared<FGamePlatformEntitlementGatewayHttpTransport, ESPMode::ThreadSafe>(Online));
        return;
    }
    if (!CurrentAccountKey.IsEmpty() || Transport.IsValid()) { ResetAccount(); }
}
