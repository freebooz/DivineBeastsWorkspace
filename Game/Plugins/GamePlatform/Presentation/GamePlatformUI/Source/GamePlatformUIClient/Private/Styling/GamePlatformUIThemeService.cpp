// 平台主题资源始终由中央Data服务签发/释放租约；本服务只持本地玩家当前与在途各一份需求。
#include "Styling/GamePlatformUIThemeService.h"
#include "Definitions/GamePlatformUIThemeDefinition.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"

void UGamePlatformUIThemeService::Initialize(UGamePlatformUIManagerSubsystem* InOwner)
{
    check(IsInGameThread());
    Owner = InOwner;
    ULocalPlayer* Player = InOwner ? InOwner->GetLocalPlayer() : nullptr;
    DataInstance = Player ? Player->GetGameInstance() : nullptr;
}

IGamePlatformDataService* UGamePlatformUIThemeService::GetDataService() const
{
    UGameInstance* Instance = DataInstance.Get();
    return IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UGamePlatformUIThemeService::ReleaseLease(FGamePlatformDataLease Lease)
{
    if (Lease.IsValid())
        if (auto* Data = GetDataService()) Data->ReleaseDefinition(Lease);
}

void UGamePlatformUIThemeService::Shutdown()
{
    check(IsInGameThread());
    if (RequestState.bClosed) return;
    RequestState.Close();
    const auto OldPending = PendingLease;
    const auto OldActive = ActiveLease;
    PendingLease = {};
    ActiveLease = {};
    PendingThemeId = {};
    ActiveDefinition = nullptr;
    // 先撤销资格再释放需求，Data取消回调不能重新装入本服务。
    ReleaseLease(OldPending);
    ReleaseLease(OldActive);
    Owner.Reset();
}

void UGamePlatformUIThemeService::NotifyFinished(FGuid RequestId, bool bSuccess, const FText& Reason)
{
    if (auto* Manager = Owner.Get())
    {
        const TStrongObjectPtr<UGamePlatformUIThemeService> KeepService(this);
        const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepManager(Manager);
        TGuardValue<bool> PublishingGuard(bPublishing, true);
        Manager->OnThemeRequestFinished.Broadcast(RequestId, bSuccess, Reason);
    }
}

bool UGamePlatformUIThemeService::CancelThemeRequest(const FGuid& RequestId)
{
    check(IsInGameThread());
    if (bPublishing || !RequestState.Cancel(RequestId)) return false;
    const auto PreviousLease = PendingLease;
    PendingLease = {};
    PendingThemeId = {};
    ReleaseLease(PreviousLease);
    NotifyFinished(RequestId, false, FText::FromString(TEXT("主题切换已取消，继续使用原主题。")));
    return true;
}

FGuid UGamePlatformUIThemeService::RequestThemeAsync(
    const FPrimaryAssetId& ThemeId, const FGamePlatformUIThemeContext& Context)
{
    check(IsInGameThread());
    if (RequestState.bClosed || bPublishing || !Context.IsValid() ||
        ThemeId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
        !ThemeId.IsValid() || Revision == MAX_int64) return {};
    auto* Data = GetDataService();
    if (!Data || !Owner.IsValid()) return {};
    const TStrongObjectPtr<UGamePlatformUIThemeService> KeepService(this);
    if (RequestState.RequestId.IsValid()) CancelThemeRequest(RequestState.RequestId);
    if (RequestState.bClosed || !Owner.IsValid()) return {};
    // 取消通知可能关停游戏实例，重新取得同一实例服务，不保留跨外部回调裸指针。
    Data = GetDataService();
    if (!Data) return {};
    const FGuid RequestId = RequestState.Begin();
    PendingThemeId = ThemeId;
    PendingContext = Context;
    const TWeakObjectPtr<UGamePlatformUIThemeService> WeakSelf(this);
    UGameInstance* Instance = Owner->GetLocalPlayer()->GetGameInstance();
    FGamePlatformResult Result;
    // Data合同保证回调在下一调度轮或更晚；句柄赋值完成后才能处理结果。
    PendingLease = Data->AcquireDefinition(ThemeId, UGamePlatformUIThemeDefinition::StaticClass(),
        {FName(TEXT("UI"))}, EGamePlatformDataLifetime::Instance, Instance,
        [WeakSelf, RequestId](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Completion)
        {
            if (auto* Self = WeakSelf.Get()) Self->HandleLoaded(RequestId, Lease, Completion);
        }, Result);
    // 即使同步拒绝，Data合同也会在下一调度轮回调失败；不能在返回GUID之前广播终态，
    // 否则调用方尚未来得及保存请求ID就会漏掉失败通知。统一由HandleLoaded结束本次请求。
    return RequestId;
}

void UGamePlatformUIThemeService::FailPending(FGuid RequestId, const FText& Reason)
{
    if (!RequestState.Cancel(RequestId)) return;
    const auto PreviousLease = PendingLease;
    PendingLease = {};
    PendingThemeId = {};
    ReleaseLease(PreviousLease);
    NotifyFinished(RequestId, false, Reason);
}

void UGamePlatformUIThemeService::HandleLoaded(FGuid RequestId,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (!RequestState.CanComplete(RequestId)) return;
    if (!Result.IsSuccess())
    {
        FailPending(RequestId, FText::FromString(FString::Printf(
            TEXT("主题资源加载失败：%s。原主题保持不变。"), *Result.Code.ToString())));
        return;
    }
    if (!PendingLease.IsValid() || Lease.ScopeId != PendingLease.ScopeId ||
        Lease.LeaseId != PendingLease.LeaseId || Lease.Generation != PendingLease.Generation ||
        Lease.DefinitionId != PendingThemeId)
    {
        FailPending(RequestId, FText::FromString(TEXT("主题加载回调的租约身份不一致。")));
        return;
    }
    auto* Data = GetDataService();
    const auto* Definition = Data && Data->GetLeaseState(PendingLease) == EGamePlatformDataRequestState::Succeeded
        ? Cast<UGamePlatformUIThemeDefinition>(Data->GetLoadedDefinition(PendingLease)) : nullptr;
    FText Error;
    if (!Definition || Definition->GetPrimaryAssetId() != PendingThemeId ||
        !Definition->ValidateLoadedStyles(PendingContext, Error))
    {
        FailPending(RequestId, Error.IsEmpty() ? FText::FromString(TEXT("主题类型、版本、样式或资源不完整。")) : Error);
        return;
    }
    const TStrongObjectPtr<UGamePlatformUIThemeService> KeepService(this);
    const auto PreviousLease = ActiveLease;
    // 先完整验证，随后原子切换所有逻辑字段，外部控件只能观察新旧完整快照。
    ActiveLease = PendingLease;
    ActiveDefinition = const_cast<UGamePlatformUIThemeDefinition*>(Definition);
    ActiveContext = PendingContext;
    PendingLease = {};
    PendingThemeId = {};
    RequestState.Cancel(RequestId);
    ++Revision;
    {
        TGuardValue<bool> PublishingGuard(bPublishing, true);
        if (auto* Manager = Owner.Get()) Manager->OnThemeChanged.Broadcast(Revision);
    }
    // 当前页面已同步收到切换事件；隐藏页通过其自身已应用样式强引用继续保活。
    ReleaseLease(PreviousLease);
    if (!RequestState.bClosed) NotifyFinished(RequestId, true, FText::GetEmpty());
}

FPrimaryAssetId UGamePlatformUIThemeService::GetCurrentThemeId() const
{
    return ActiveDefinition ? ActiveDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
}

UClass* UGamePlatformUIThemeService::ResolveStyle(
    EGamePlatformUIStyleKind Kind, FName Key, bool bFallback, FText& Error) const
{
    check(IsInGameThread());
    auto* Data = GetDataService();
    if (RequestState.bClosed || !ActiveDefinition || !Data ||
        Data->GetLeaseState(ActiveLease) != EGamePlatformDataRequestState::Succeeded)
    {
        Error = FText::FromString(TEXT("当前本地玩家尚无可用主题，保留原控件样式。"));
        return nullptr;
    }
    return ActiveDefinition->ResolveLoadedStyle(Kind, Key, ActiveContext, bFallback, Error);
}
