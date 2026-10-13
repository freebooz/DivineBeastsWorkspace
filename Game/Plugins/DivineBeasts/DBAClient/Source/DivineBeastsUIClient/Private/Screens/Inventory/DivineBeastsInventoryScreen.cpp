#include "Screens/Inventory/DivineBeastsInventoryScreen.h"

#include "Engine/LocalPlayer.h"
// 背包领域中文提示的稳定本地化命名空间，仅供项目视觉层使用。
#define LOCTEXT_NAMESPACE "DivineBeastsInventoryScreen"


namespace
{
const TArray<FGamePlatformInventoryItemViewModel>& EmptyInventoryItems()
{
    static const TArray<FGamePlatformInventoryItemViewModel> Empty;
    return Empty;
}
}

EGamePlatformInventoryClientState
UDivineBeastsInventoryScreen::GetInventoryState() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetState()
        : EGamePlatformInventoryClientState::Uninitialized;
}

EGamePlatformInventoryError
UDivineBeastsInventoryScreen::GetInventoryLastError() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetLastError()
        : EGamePlatformInventoryError::None;
}

int64 UDivineBeastsInventoryScreen::GetInventoryRevision() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetInventoryRevision()
        : 0;
}


FText UDivineBeastsInventoryScreen::GetInventoryStateText() const
{
    // 正式本地化键采用LOCTEXT，状态变化由平台OnChanged推送而不是蓝图逐帧翻译。
    switch (GetInventoryState())
    {
    case EGamePlatformInventoryClientState::Uninitialized:
        return LOCTEXT("InventoryUninitialized", "尚未登录或背包未初始化");
    case EGamePlatformInventoryClientState::Loading:
        return LOCTEXT("InventoryLoading", "正在读取背包…");
    case EGamePlatformInventoryClientState::Ready:
        return LOCTEXT("InventoryReady", "背包已同步");
    case EGamePlatformInventoryClientState::Mutating:
        return LOCTEXT("InventoryMutating", "正在提交操作…");
    case EGamePlatformInventoryClientState::Reconciling:
        return LOCTEXT("InventoryReconciling", "正在核对服务器结果…");
    case EGamePlatformInventoryClientState::Error:
        return LOCTEXT("InventoryError", "背包暂时不可用");
    default:
        return LOCTEXT("InventoryUnknown", "背包状态未知");
    }
}

FText UDivineBeastsInventoryScreen::GetInventoryErrorText() const
{
    // 只映射平台约定的机器错误，不显示可能包含敏感信息的后端原始消息。
    switch (GetInventoryLastError())
    {
    case EGamePlatformInventoryError::None: return FText::GetEmpty();
    case EGamePlatformInventoryError::InventoryNotLoaded:
        return LOCTEXT("InventoryNotLoaded", "背包尚未加载");
    case EGamePlatformInventoryError::ItemNotFound:
        return LOCTEXT("InventoryItemNotFound", "物品不存在或已变化，请刷新背包");
    case EGamePlatformInventoryError::DefinitionNotFound:
        return LOCTEXT("InventoryDefinitionNotFound", "物品显示资料不可用");
    case EGamePlatformInventoryError::InvalidQuantity:
        return LOCTEXT("InventoryInvalidQuantity", "物品数量不合法");
    case EGamePlatformInventoryError::StackNotSupported:
        return LOCTEXT("InventoryStackNotSupported", "该物品不支持拆分或堆叠");
    case EGamePlatformInventoryError::StackLimitExceeded:
        return LOCTEXT("InventoryStackLimitExceeded", "超出物品堆叠上限");
    case EGamePlatformInventoryError::SlotOutOfRange:
        return LOCTEXT("InventorySlotOutOfRange", "背包槽位超出当前容量");
    case EGamePlatformInventoryError::SlotOccupied:
        return LOCTEXT("InventorySlotOccupied", "目标位置已有物品");
    case EGamePlatformInventoryError::ContainerNotFound:
        return LOCTEXT("InventoryContainerNotFound", "背包容器不存在");
    case EGamePlatformInventoryError::RevisionConflict:
        return LOCTEXT("InventoryRevisionConflict", "背包已更新，正在重新同步");
    case EGamePlatformInventoryError::DuplicateOperation:
        return LOCTEXT("InventoryDuplicateOperation", "该背包操作已提交");
    case EGamePlatformInventoryError::OperationInProgress:
        return LOCTEXT("InventoryOperationInProgress", "上一项背包操作尚未完成");
    case EGamePlatformInventoryError::OperationNotFound:
        return LOCTEXT("InventoryOperationNotFound", "服务器未找到原操作，正在恢复");
    case EGamePlatformInventoryError::InventoryFull:
        return LOCTEXT("InventoryFull", "背包空间不足");
    case EGamePlatformInventoryError::ConsumeNotAllowed:
        return LOCTEXT("InventoryConsumeNotAllowed", "当前物品不可使用");
    case EGamePlatformInventoryError::InsufficientQuantity:
        return LOCTEXT("InventoryInsufficientQuantity", "物品数量不足");
    case EGamePlatformInventoryError::OutcomeUnknown:
        return LOCTEXT("InventoryOutcomeUnknown", "结果尚未确认，请查询原操作，勿重复提交");
    case EGamePlatformInventoryError::BackendUnavailable:
        return LOCTEXT("InventoryBackendUnavailable", "背包服务器暂不可用");
    case EGamePlatformInventoryError::Unauthorized:
        return LOCTEXT("InventoryUnauthorized", "登录失效或操作无权限");
    case EGamePlatformInventoryError::Cancelled:
        return LOCTEXT("InventoryCancelled", "已取消等待，服务器结果仍需核对");
    case EGamePlatformInventoryError::TimedOut:
        return LOCTEXT("InventoryTimedOut", "请求超时，请核对原操作");
    case EGamePlatformInventoryError::InvalidResponse:
        return LOCTEXT("InventoryInvalidResponse", "背包数据校验失败");
    default:
        return LOCTEXT("InventoryUnknownError", "背包操作失败");
    }
}

bool UDivineBeastsInventoryScreen::CanModifyInventory() const
{
    return GetInventoryState() == EGamePlatformInventoryClientState::Ready &&
           !HasPendingInventoryOperation() && GetInventoryRevision() > 0;
}

bool UDivineBeastsInventoryScreen::HasPendingInventoryOperation() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem) &&
        Subsystem->HasPendingOperation();
}

TArray<FGamePlatformInventoryItemViewModel>
UDivineBeastsInventoryScreen::GetInventoryItems() const
{
    return GetInventoryItemsView();
}

const TArray<FGamePlatformInventoryItemViewModel>&
UDivineBeastsInventoryScreen::GetInventoryItemsView() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetViewModelsView()
        : EmptyInventoryItems();
}


TArray<FGamePlatformInventoryContainerSnapshot>
UDivineBeastsInventoryScreen::GetInventoryContainers() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem) ? Subsystem->GetSnapshot().Containers
                              : TArray<FGamePlatformInventoryContainerSnapshot>();
}

int32 UDivineBeastsInventoryScreen::GetInventoryContainerCapacity(FName ContainerId) const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem) ? Subsystem->GetContainerCapacity(ContainerId) : 0;
}

bool UDivineBeastsInventoryScreen::TryGetInventorySlot(
    FName ContainerId, int32 SlotIndex, FGamePlatformInventorySlotViewModel& OutSlot) const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    if (!IsValid(Subsystem))
    {
        OutSlot = FGamePlatformInventorySlotViewModel();
        return false;
    }
    return Subsystem->TryGetContainerSlot(ContainerId, SlotIndex, OutSlot);
}

TArray<FGamePlatformInventorySlotViewModel>
UDivineBeastsInventoryScreen::GetInventorySlotPage(
    FName ContainerId, int32 StartSlotIndex, int32 MaxSlotCount) const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem) ? Subsystem->GetContainerSlotPage(
        ContainerId, StartSlotIndex, MaxSlotCount)
                              : TArray<FGamePlatformInventorySlotViewModel>();
}

TArray<FGamePlatformInventoryQuickbarViewModel>
UDivineBeastsInventoryScreen::GetInventoryQuickbarSlots() const
{
    const UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem) ? Subsystem->GetQuickbarSlots()
                              : TArray<FGamePlatformInventoryQuickbarViewModel>();
}

bool UDivineBeastsInventoryScreen::RefreshInventory()
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem) &&
        Subsystem->RefreshSnapshot();
}


bool UDivineBeastsInventoryScreen::RetryPendingInventoryOperation()
{
    UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem) && Subsystem->RetryPendingOperation();
}

FGuid UDivineBeastsInventoryScreen::RequestMoveItem(
    const FString& ItemInstanceId,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestMove(
            ItemInstanceId,
            TargetContainerId,
            TargetSlotIndex)
        : FGuid();
}

FGuid UDivineBeastsInventoryScreen::RequestSplitItem(
    const FString& SourceItemInstanceId,
    int32 SplitQuantity,
    FName TargetContainerId,
    int32 TargetSlotIndex)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestSplit(
            SourceItemInstanceId,
            SplitQuantity,
            TargetContainerId,
            TargetSlotIndex)
        : FGuid();
}

FGuid UDivineBeastsInventoryScreen::RequestMergeItem(
    const FString& SourceItemInstanceId,
    const FString& TargetItemInstanceId)
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestMerge(
            SourceItemInstanceId,
            TargetItemInstanceId)
        : FGuid();
}



FGuid UDivineBeastsInventoryScreen::RequestDropInventoryItem(
    const FString& SourceItemInstanceId,
    FName TargetContainerId,
    int32 TargetSlotIndex,
    bool bPreferMerge)
{
    // 局部校验只减少无效UI请求；任何实际写入仍由PlayerData校验身份、版本与数量。
    UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    if (!IsValid(Subsystem) || !CanModifyInventory() || SourceItemInstanceId.IsEmpty())
    {
        return {};
    }
    const FGamePlatformInventoryItemInstance* Source =
        Subsystem->FindItem(SourceItemInstanceId);
    if (!Source || (Source->ContainerId == TargetContainerId &&
                    Source->SlotIndex == TargetSlotIndex))
    {
        return {};
    }

    FGamePlatformInventorySlotViewModel Target;
    if (!Subsystem->TryGetContainerSlot(TargetContainerId, TargetSlotIndex, Target))
    {
        return {};
    }

    if (Target.bOccupied && bPreferMerge &&
        Target.Item.ItemDefinitionId == Source->ItemDefinitionId)
    {
        // 同定义拖放优先整堆合并，禁止超过后端快照公布的上限后偷偷交换。
        const int64 CombinedQuantity =
            static_cast<int64>(Source->Quantity) + Target.Item.Quantity;
        if (Target.Item.ItemInstanceId == SourceItemInstanceId ||
            Target.Item.MaxStackSize <= 1 ||
            CombinedQuantity > Target.Item.MaxStackSize)
        {
            return {};
        }
        return Subsystem->RequestMerge(SourceItemInstanceId, Target.Item.ItemInstanceId);
    }

    // 目标空槽或不同定义物品交由权威服务判断是移动还是交换。
    return Subsystem->RequestMove(SourceItemInstanceId, TargetContainerId, TargetSlotIndex);
}

FGuid UDivineBeastsInventoryScreen::RequestSetInventoryQuickbar(
    int32 SlotIndex, const FString& ItemInstanceId)
{
    UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestSetQuickbar(SlotIndex, ItemInstanceId)
        : FGuid();
}

FGuid UDivineBeastsInventoryScreen::RequestClearInventoryQuickbar(int32 SlotIndex)
{
    UGamePlatformInventoryClientSubsystem* Subsystem = ResolveInventorySubsystem();
    return IsValid(Subsystem)
        ? Subsystem->RequestClearQuickbar(SlotIndex)
        : FGuid();
}

void UDivineBeastsInventoryScreen::BindUIEvents()
{
    Super::BindUIEvents();

    InventorySubsystem = ResolveInventorySubsystem();
    if (IsValid(InventorySubsystem) &&
        !InventoryChangedHandle.IsValid())
    {
        InventoryChangedHandle =
            InventorySubsystem->OnChanged.AddUObject(
                this,
                &UDivineBeastsInventoryScreen::HandleInventoryChanged);
    }
}

void UDivineBeastsInventoryScreen::UnbindUIEvents()
{
    if (IsValid(InventorySubsystem) &&
        InventoryChangedHandle.IsValid())
    {
        InventorySubsystem->OnChanged.Remove(InventoryChangedHandle);
        InventoryChangedHandle.Reset();
    }
    InventorySubsystem = nullptr;

    Super::UnbindUIEvents();
}

void UDivineBeastsInventoryScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    HandleInventoryChanged();
}

void UDivineBeastsInventoryScreen::HandleInventoryChanged()
{
    UGamePlatformInventoryClientSubsystem* Subsystem =
        ResolveInventorySubsystem();
    if (!IsValid(Subsystem))
    {
        BP_OnInventoryViewChanged(
            0,
            EGamePlatformInventoryClientState::Uninitialized,
            false);
        return;
    }

    BP_OnInventoryViewChanged(
        Subsystem->GetInventoryRevision(),
        Subsystem->GetState(),
        Subsystem->HasPendingOperation());
}

UGamePlatformInventoryClientSubsystem*
UDivineBeastsInventoryScreen::ResolveInventorySubsystem() const
{
    if (IsValid(InventorySubsystem))
    {
        return InventorySubsystem;
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    return IsValid(LocalPlayer)
        ? LocalPlayer->GetSubsystem<UGamePlatformInventoryClientSubsystem>()
        : nullptr;
}

#undef LOCTEXT_NAMESPACE
