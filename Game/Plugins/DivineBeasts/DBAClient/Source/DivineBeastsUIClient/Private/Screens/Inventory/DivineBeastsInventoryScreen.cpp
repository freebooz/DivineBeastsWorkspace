#include "Screens/Inventory/DivineBeastsInventoryScreen.h"
#include "Screens/Inventory/DivineBeastsInventoryPresentationCatalog.h"

#include "Engine/LocalPlayer.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "UObject/StrongObjectPtr.h"

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

// 每个可见格拥有专用的无参数Button点击回调转接；弱引用确保跨世界退出无悬挂页面。
void UDivineBeastsInventoryButtonProxy::Bind(
    UDivineBeastsInventoryScreen* Screen, UButton* Button, int32 Index, bool bQuickbar)
{
    Unbind();
    if (!IsValid(Screen) || !IsValid(Button) || Index < 0)
    {
        return;
    }
    ScreenOwner = Screen;
    ButtonOwner = Button;
    LocalIndex = Index;
    bIsQuickbar = bQuickbar;
    Button->OnClicked.AddUniqueDynamic(this, &UDivineBeastsInventoryButtonProxy::HandleButtonClicked);
}

void UDivineBeastsInventoryButtonProxy::Unbind()
{
    if (UButton* Button = ButtonOwner.Get())
    {
        Button->OnClicked.RemoveDynamic(this, &UDivineBeastsInventoryButtonProxy::HandleButtonClicked);
    }
    ScreenOwner.Reset();
    ButtonOwner.Reset();
    LocalIndex = INDEX_NONE;
    bIsQuickbar = false;
}

void UDivineBeastsInventoryButtonProxy::HandleButtonClicked()
{
    if (UDivineBeastsInventoryScreen* Screen = ScreenOwner.Get();
        IsValid(Screen) && Screen->IsActivated())
    {
        Screen->HandleInventoryCellClicked(LocalIndex, bIsQuickbar);
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


void UDivineBeastsInventoryScreen::BindNamedActionButtons()
{
    // 命名按钮均由Monolith真实资产拥有；仅为其添加业务请求代理，不动态构造界面树。
#define DBA_BIND_INVENTORY_BUTTON(Name, Method) \
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT(Name)))) \
    { Button->OnClicked.AddUniqueDynamic(this, &UDivineBeastsInventoryScreen::Method); }
    DBA_BIND_INVENTORY_BUTTON("CloseButton", HandleCloseButtonClicked)
    DBA_BIND_INVENTORY_BUTTON("PrevPageButton", HandlePrevPageClicked)
    DBA_BIND_INVENTORY_BUTTON("NextPageButton", HandleNextPageClicked)
    DBA_BIND_INVENTORY_BUTTON("RefreshButton", HandleRefreshClicked)
    DBA_BIND_INVENTORY_BUTTON("RetryButton", HandleRetryClicked)
    DBA_BIND_INVENTORY_BUTTON("MoveButton", HandleMoveClicked)
    DBA_BIND_INVENTORY_BUTTON("SplitButton", HandleSplitClicked)
    DBA_BIND_INVENTORY_BUTTON("MergeButton", HandleMergeClicked)
    DBA_BIND_INVENTORY_BUTTON("BindQuickbarButton", HandleBindQuickbarClicked)
    DBA_BIND_INVENTORY_BUTTON("ClearQuickbarButton", HandleClearQuickbarClicked)
#undef DBA_BIND_INVENTORY_BUTTON
}

void UDivineBeastsInventoryScreen::UnbindNamedActionButtons()
{
    // 不清除他人持有的监听器：只撤本页面注册的准确目标函数。
#define DBA_UNBIND_INVENTORY_BUTTON(Name, Method) \
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT(Name)))) \
    { Button->OnClicked.RemoveDynamic(this, &UDivineBeastsInventoryScreen::Method); }
    DBA_UNBIND_INVENTORY_BUTTON("CloseButton", HandleCloseButtonClicked)
    DBA_UNBIND_INVENTORY_BUTTON("PrevPageButton", HandlePrevPageClicked)
    DBA_UNBIND_INVENTORY_BUTTON("NextPageButton", HandleNextPageClicked)
    DBA_UNBIND_INVENTORY_BUTTON("RefreshButton", HandleRefreshClicked)
    DBA_UNBIND_INVENTORY_BUTTON("RetryButton", HandleRetryClicked)
    DBA_UNBIND_INVENTORY_BUTTON("MoveButton", HandleMoveClicked)
    DBA_UNBIND_INVENTORY_BUTTON("SplitButton", HandleSplitClicked)
    DBA_UNBIND_INVENTORY_BUTTON("MergeButton", HandleMergeClicked)
    DBA_UNBIND_INVENTORY_BUTTON("BindQuickbarButton", HandleBindQuickbarClicked)
    DBA_UNBIND_INVENTORY_BUTTON("ClearQuickbarButton", HandleClearQuickbarClicked)
#undef DBA_UNBIND_INVENTORY_BUTTON
}


void UDivineBeastsInventoryScreen::CancelInventoryIconLoads()
{
    ++IconLoadGeneration;
    for (auto& Pair : PendingIconLoads)
    {
        FGamePlatformAssetLoader::Cancel(Pair.Value);
    }
    PendingIconLoads.Reset();
    AttemptedIconDefinitions.Reset();
    // 关闭/换账号时卸下图标画刷强引用，避免复用Widget时短暂显示上一玩家的物品。
    if (bVisualControlsBound && IsValid(this) && !IsUnreachable())
    {
        for (int32 Index = 0; Index < SlotsPerPage + 12; ++Index)
        {
            const bool bQuickbar = Index >= SlotsPerPage;
            const int32 LocalIndex = bQuickbar ? Index - SlotsPerPage : Index;
            const FString IconName = bQuickbar
                ? FString::Printf(TEXT("QuickIcon_%d"), LocalIndex)
                : FString::Printf(TEXT("SlotIcon_%d"), LocalIndex);
            if (UImage* Image = Cast<UImage>(GetWidgetFromName(FName(*IconName))))
            {
                Image->SetBrushFromTexture(nullptr);
                Image->SetVisibility(ESlateVisibility::Hidden);
            }
        }
    }
}

void UDivineBeastsInventoryScreen::RequestInventoryIcon(FName DefinitionId)
{
    if (!bVisualControlsBound || !IsActivated() || DefinitionId.IsNone() ||
        AttemptedIconDefinitions.Contains(DefinitionId))
    {
        return;
    }
    const FSoftObjectPath Path =
        FDivineBeastsInventoryPresentationCatalog::Find(DefinitionId).IconAssetPath;
    if (!Path.IsValid())
    {
        return;
    }

    AttemptedIconDefinitions.Add(DefinitionId);
    const uint64 ExpectedGeneration = IconLoadGeneration;
    const TWeakObjectPtr<UDivineBeastsInventoryScreen> WeakThis(this);
    // 与头像/小地图共用平台AssetLoader（资源加载器），不使用LoadSynchronous或逐帧查找。
    PendingIconLoads.Add(DefinitionId, FGamePlatformAssetLoader::RequestAsyncLoad(
        {Path}, FStreamableDelegate::CreateLambda(
            [WeakThis, ExpectedGeneration]()
            {
                if (UDivineBeastsInventoryScreen* Screen = WeakThis.Get();
                    IsValid(Screen) && !Screen->IsUnreachable() &&
                    Screen->IconLoadGeneration == ExpectedGeneration &&
                    Screen->bVisualControlsBound && Screen->IsActivated())
                {
                    Screen->RefreshInventoryVisuals();
                }
            })));
}

void UDivineBeastsInventoryScreen::RenderInventoryIcon(
    const FString& WidgetName, FName DefinitionId)
{
    // 只有Monolith真正制作SlotIcon/QuickIcon时才异步请求资源；
    // 原始172控件背包蓝图没有这些控件时只显示中文文字，不增加空白纹理或同步磁盘查询。
    UImage* IconWidget = Cast<UImage>(GetWidgetFromName(FName(*WidgetName)));
    if (!IsValid(IconWidget))
    {
        return;
    }

    const FSoftObjectPath Path = !DefinitionId.IsNone()
        ? FDivineBeastsInventoryPresentationCatalog::Find(DefinitionId).IconAssetPath
        : FSoftObjectPath();
    UTexture2D* Texture = Path.IsValid()
        ? Cast<UTexture2D>(Path.ResolveObject())
        : nullptr;
    if (IsValid(Texture))
    {
        IconWidget->SetBrushFromTexture(Texture);
        IconWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        return;
    }
    IconWidget->SetBrushFromTexture(nullptr);
    IconWidget->SetVisibility(ESlateVisibility::Hidden);
    if (Path.IsValid())
    {
        RequestInventoryIcon(DefinitionId);
    }
}

void UDivineBeastsInventoryScreen::BindInventoryVisualControls()
{
    if (bVisualControlsBound || !IsActivated())
    {
        return;
    }

    bVisualControlsBound = true;
    SlotButtonProxies.Reserve(SlotsPerPage + 12);
    for (int32 Index = 0; Index < SlotsPerPage + 12; ++Index)
    {
        const bool bQuickbar = Index >= SlotsPerPage;
        const int32 LocalIndex = bQuickbar ? Index - SlotsPerPage : Index;
        // UE的Printf格式必须使用静态格式字面量，避免动态格式与错误归属。
        const FString Name = bQuickbar
            ? FString::Printf(TEXT("QuickButton_%d"), LocalIndex)
            : FString::Printf(TEXT("SlotButton_%d"), LocalIndex);
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*Name))))
        {
            UDivineBeastsInventoryButtonProxy* Proxy =
                NewObject<UDivineBeastsInventoryButtonProxy>(this);
            Proxy->Bind(this, Button, LocalIndex, bQuickbar);
            SlotButtonProxies.Add(Proxy);
        }
    }
    BindNamedActionButtons();
}

void UDivineBeastsInventoryScreen::UnbindInventoryVisualControls()
{
    CancelInventoryIconLoads();
    if (!bVisualControlsBound)
    {
        return;
    }
    bVisualControlsBound = false;
    UnbindNamedActionButtons();
    for (UDivineBeastsInventoryButtonProxy* Proxy : SlotButtonProxies)
    {
        if (IsValid(Proxy)) { Proxy->Unbind(); }
    }
    SlotButtonProxies.Reset();
    // 页面重新激活或玩家切号都需要重新选择来源；不得保留旧账号物品身份。
    SelectedItemInstanceId.Reset();
    SelectedQuickbarIndex = 0;
    TargetAction = ETargetAction::None;
    ActionHint = FText::GetEmpty();
    PageIndex = 0;
}

void UDivineBeastsInventoryScreen::SetInventoryHint(const FText& Hint)
{
    ActionHint = Hint;
    RefreshInventoryVisuals();
}

void UDivineBeastsInventoryScreen::RefreshInventoryVisuals()
{
    if (!bVisualControlsBound || bRefreshingVisuals)
    {
        return;
    }
    TGuardValue<bool> RefreshGuard(bRefreshingVisuals, true);
    const UGamePlatformInventoryClientSubsystem* Service = ResolveInventorySubsystem();
    const int32 Capacity = IsValid(Service)
        ? Service->GetContainerCapacity(SelectedContainerId) : 0;
    PageIndex = Capacity > 0 ? FMath::Clamp(PageIndex, 0, (Capacity - 1) / SlotsPerPage) : 0;
    const bool bReady = CanModifyInventory();
    const int32 PageStart = PageIndex * SlotsPerPage;

    const TArray<FGamePlatformInventorySlotViewModel> Slots = IsValid(Service)
        ? Service->GetContainerSlotPage(SelectedContainerId, PageStart, SlotsPerPage)
        : TArray<FGamePlatformInventorySlotViewModel>();
    const TArray<FGamePlatformInventoryQuickbarViewModel> Quickbar = IsValid(Service)
        ? Service->GetQuickbarSlots()
        : TArray<FGamePlatformInventoryQuickbarViewModel>();

    const auto SetText = [this](const FString& WidgetName, const FText& Value)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(FName(*WidgetName))))
        {
            Label->SetText(Value);
        }
    };
    const auto SetButtonEnabled = [this](const TCHAR* Name, bool bEnabled)
    {
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)))
        {
            Button->SetIsEnabled(bEnabled);
        }
    };
    // 仅遍历服务器快照一次求已占用格数，不构造整份物品副本。
    int32 OccupiedCount = 0;
    if (IsValid(Service) && Capacity > 0)
    {
        for (const FGamePlatformInventoryItemInstance& Item : Service->GetSnapshot().Items)
        {
            if (Item.ContainerId == SelectedContainerId) { ++OccupiedCount; }
        }
    }
    SetText(TEXT("CapacityText"), FText::Format(
        LOCTEXT("InventoryCapacity", "已占用 {0} / {1} 格"),
        FText::AsNumber(OccupiedCount), FText::AsNumber(Capacity)));
    SetText(TEXT("StatusText"), ActionHint.IsEmpty()
        ? GetInventoryStateText()
        : FText::Format(LOCTEXT("InventoryStatusAndHint", "{0} · {1}"),
                        GetInventoryStateText(), ActionHint));
    SetText(TEXT("ErrorText"), GetInventoryErrorText());
    SetText(TEXT("PageIndicator"), FText::Format(
        LOCTEXT("InventoryPage", "第 {0} / {1} 页"),
        FText::AsNumber(PageIndex + 1),
        FText::AsNumber(FMath::Max(1, (Capacity + SlotsPerPage - 1) / SlotsPerPage))));
    SetButtonEnabled(TEXT("PrevPageButton"), PageIndex > 0);
    SetButtonEnabled(TEXT("NextPageButton"), PageStart + SlotsPerPage < Capacity);

    for (int32 Index = 0; Index < SlotsPerPage; ++Index)
    {
        const FGamePlatformInventorySlotViewModel* ProjectedSlot =
            Slots.IsValidIndex(Index) ? &Slots[Index] : nullptr;
        const bool bOccupied = ProjectedSlot && ProjectedSlot->bOccupied;
        FString LabelText;
        if (!ProjectedSlot) { LabelText = TEXT(""); }
        else if (!bOccupied)
        {
            LabelText = FString::Printf(TEXT("%02d"), ProjectedSlot->SlotIndex + 1);
        }
        else
        {
            // 图标目录尚未完成时仅展示权威物品定义短标识，完整标识放入工具提示。
            const FString DefinitionName = FDivineBeastsInventoryPresentationCatalog::Find(
                ProjectedSlot->Item.ItemDefinitionId).DisplayName.ToString();
            LabelText = DefinitionName.Left(7) +
                FString::Printf(TEXT("\n×%d"), ProjectedSlot->Item.Quantity);
            if (ProjectedSlot->Item.bPending) { LabelText += TEXT("…"); }
        }
        SetText(FString::Printf(TEXT("SlotText_%d"), Index), FText::FromString(LabelText));
        RenderInventoryIcon(FString::Printf(TEXT("SlotIcon_%d"), Index),
            bOccupied ? ProjectedSlot->Item.ItemDefinitionId : NAME_None);
        const FString ButtonName = FString::Printf(TEXT("SlotButton_%d"), Index);
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName))))
        {
            Button->SetIsEnabled(ProjectedSlot && bReady);
            Button->SetBackgroundColor(
                bOccupied && ProjectedSlot->Item.ItemInstanceId == SelectedItemInstanceId
                ? FLinearColor(0.34f, 0.55f, 0.40f, 1.0f)
                : FLinearColor(0.12f, 0.24f, 0.22f, 1.0f));
            Button->SetToolTipText(
                bOccupied
                ? FDivineBeastsInventoryPresentationCatalog::Find(ProjectedSlot->Item.ItemDefinitionId).DisplayName
                : LOCTEXT("InventoryEmptySlot", "空背包格"));
        }
    }

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const FGamePlatformInventoryQuickbarViewModel* Quick =
            Quickbar.IsValidIndex(Index) ? &Quickbar[Index] : nullptr;
        FString LabelText = FString::FromInt(Index + 1);
        if (Quick && Quick->bAssigned)
        {
            LabelText = FDivineBeastsInventoryPresentationCatalog::Find(
                Quick->Item.ItemDefinitionId).DisplayName.ToString().Left(7) +
                FString::Printf(TEXT("\n×%d"), Quick->Item.Quantity);
        }
        if (Quick && Quick->bPending) { LabelText += TEXT("…"); }
        SetText(FString::Printf(TEXT("QuickText_%d"), Index), FText::FromString(LabelText));
        RenderInventoryIcon(FString::Printf(TEXT("QuickIcon_%d"), Index),
            Quick && Quick->bAssigned ? Quick->Item.ItemDefinitionId : NAME_None);
        const FString ButtonName = FString::Printf(TEXT("QuickButton_%d"), Index);
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(FName(*ButtonName))))
        {
            Button->SetIsEnabled(bReady);
            Button->SetBackgroundColor(Index == SelectedQuickbarIndex
                ? FLinearColor(0.38f, 0.48f, 0.28f, 1.0f)
                : FLinearColor(0.14f, 0.24f, 0.23f, 1.0f));
        }
    }

    const FGamePlatformInventoryItemInstance* Selected = IsValid(Service) &&
        !SelectedItemInstanceId.IsEmpty() ? Service->FindItem(SelectedItemInstanceId) : nullptr;
    SetText(TEXT("SelectedItemName"), Selected
        ? FDivineBeastsInventoryPresentationCatalog::Find(Selected->ItemDefinitionId).DisplayName
        : LOCTEXT("InventorySelectItem", "请选择物品"));
    SetText(TEXT("SelectedItemCount"), Selected
        ? FText::Format(LOCTEXT("InventorySelectedQuantity", "数量 {0} · 堆叠上限 {1}"),
            FText::AsNumber(Selected->Quantity), FText::AsNumber(Selected->MaxStackSize))
        : FText::GetEmpty());
    SetText(TEXT("SelectedItemDescription"), Selected
        ? FDivineBeastsInventoryPresentationCatalog::Find(Selected->ItemDefinitionId).Description
        : LOCTEXT("InventorySelectHint", "选择物品格，可查看数量并进行整理操作。"));
    SetButtonEnabled(TEXT("MoveButton"), bReady && Selected);
    SetButtonEnabled(TEXT("SplitButton"), bReady && Selected && Selected->Quantity > 1);
    SetButtonEnabled(TEXT("MergeButton"), bReady && Selected && Selected->MaxStackSize > 1);
    SetButtonEnabled(TEXT("BindQuickbarButton"), bReady && Selected);
    SetButtonEnabled(TEXT("ClearQuickbarButton"), bReady &&
        Quickbar.IsValidIndex(SelectedQuickbarIndex) &&
        Quickbar[SelectedQuickbarIndex].bAssigned);
    SetButtonEnabled(TEXT("RefreshButton"), IsValid(Service) &&
        !Service->HasPendingOperation() &&
        (Service->GetState() == EGamePlatformInventoryClientState::Ready ||
         Service->GetState() == EGamePlatformInventoryClientState::Error));
    SetButtonEnabled(TEXT("RetryButton"), IsValid(Service) &&
        Service->HasPendingOperation() &&
        Service->GetState() == EGamePlatformInventoryClientState::Error);
}


void UDivineBeastsInventoryScreen::HandleInventoryCellClicked(int32 LocalIndex, bool bQuickbar)
{
    if (!IsActivated() || !CanModifyInventory())
    {
        return;
    }
    UGamePlatformInventoryClientSubsystem* Service = ResolveInventorySubsystem();
    if (!IsValid(Service))
    {
        return;
    }

    if (bQuickbar)
    {
        if (LocalIndex < 0 || LocalIndex >= 12) { return; }
        SelectedQuickbarIndex = LocalIndex;
        TargetAction = ETargetAction::None;
        const TArray<FGamePlatformInventoryQuickbarViewModel> Quickbar = Service->GetQuickbarSlots();
        if (SelectedItemInstanceId.IsEmpty() &&
            Quickbar.IsValidIndex(LocalIndex) && Quickbar[LocalIndex].bAssigned)
        {
            SelectedItemInstanceId = Quickbar[LocalIndex].Item.ItemInstanceId;
        }
        SetInventoryHint(FText::Format(
            LOCTEXT("InventoryQuickbarSelected", "已选择快捷栏第 {0} 格"),
            FText::AsNumber(LocalIndex + 1)));
        return;
    }

    if (LocalIndex < 0 || LocalIndex >= SlotsPerPage) { return; }
    const int32 AbsoluteSlot = PageIndex * SlotsPerPage + LocalIndex;
    FGamePlatformInventorySlotViewModel Target;
    if (!Service->TryGetContainerSlot(SelectedContainerId, AbsoluteSlot, Target))
    {
        return;
    }

    if (TargetAction != ETargetAction::None &&
        !SelectedItemInstanceId.IsEmpty())
    {
        const FGamePlatformInventoryItemInstance* Source = Service->FindItem(SelectedItemInstanceId);
        if (!Source)
        {
            SelectedItemInstanceId.Reset();
            TargetAction = ETargetAction::None;
            SetInventoryHint(LOCTEXT("InventorySourceGone", "物品已变化，请重新选择。"));
            return;
        }
        FGuid Accepted;
        switch (TargetAction)
        {
        case ETargetAction::Move:
            Accepted = RequestDropInventoryItem(
                SelectedItemInstanceId, SelectedContainerId, AbsoluteSlot, false);
            break;
        case ETargetAction::Merge:
            if (Target.bOccupied && Target.Item.ItemInstanceId != SelectedItemInstanceId &&
                Target.Item.ItemDefinitionId == Source->ItemDefinitionId)
            {
                Accepted = RequestMergeItem(SelectedItemInstanceId, Target.Item.ItemInstanceId);
            }
            break;
        case ETargetAction::Split:
            // 常用整理快捷操作：拆出半堆到空格；精确数量仍可走已有Blueprint命令参数接口。
            if (!Target.bOccupied && Source->Quantity > 1)
            {
                Accepted = RequestSplitItem(
                    SelectedItemInstanceId, FMath::Max(1, Source->Quantity / 2),
                    SelectedContainerId, AbsoluteSlot);
            }
            break;
        default: break;
        }
        if (Accepted.IsValid())
        {
            TargetAction = ETargetAction::None;
            SetInventoryHint(LOCTEXT("InventoryCommandAccepted", "已提交操作，等待服务器确认。"));
        }
        else
        {
            SetInventoryHint(LOCTEXT("InventoryInvalidTarget", "目标格不满足操作条件，请重新选择。"));
        }
        return;
    }

    SelectedItemInstanceId = Target.bOccupied ? Target.Item.ItemInstanceId : FString();
    SetInventoryHint(Target.bOccupied
        ? LOCTEXT("InventoryItemSelected", "已选中物品，可选择移动、拆分、合并或快捷栏。")
        : LOCTEXT("InventorySlotEmpty", "这是空背包格。"));
}

void UDivineBeastsInventoryScreen::HandleCloseButtonClicked()
{
    // 平台层统一关闭页面，不能直接RemoveFromParent绕过CommonUI层栈。
    CloseScreen();
}

void UDivineBeastsInventoryScreen::HandlePrevPageClicked()
{
    if (PageIndex > 0) { --PageIndex; RefreshInventoryVisuals(); }
}

void UDivineBeastsInventoryScreen::HandleNextPageClicked()
{
    const int32 Capacity = GetInventoryContainerCapacity(SelectedContainerId);
    if ((PageIndex + 1) * SlotsPerPage < Capacity)
    {
        ++PageIndex;
        RefreshInventoryVisuals();
    }
}

void UDivineBeastsInventoryScreen::HandleRefreshClicked()
{
    if (RefreshInventory()) { SetInventoryHint(LOCTEXT("InventoryRefreshing", "正在向服务器读取完整背包快照。")); }
}

void UDivineBeastsInventoryScreen::HandleRetryClicked()
{
    if (HasPendingInventoryOperation() && RetryPendingInventoryOperation())
    {
        SetInventoryHint(LOCTEXT("InventoryRetryOriginal", "已按原操作编号查询服务器结果。"));
    }
}

void UDivineBeastsInventoryScreen::HandleMoveClicked()
{
    if (!CanModifyInventory() || SelectedItemInstanceId.IsEmpty()) { return; }
    TargetAction = ETargetAction::Move;
    SetInventoryHint(LOCTEXT("InventoryMoveTarget", "请选择目标物品格（目标已有物品时交换位置）。"));
}

void UDivineBeastsInventoryScreen::HandleSplitClicked()
{
    const UGamePlatformInventoryClientSubsystem* Service = ResolveInventorySubsystem();
    const FGamePlatformInventoryItemInstance* Source =
        IsValid(Service) ? Service->FindItem(SelectedItemInstanceId) : nullptr;
    if (!CanModifyInventory() || !Source || Source->Quantity <= 1) { return; }
    TargetAction = ETargetAction::Split;
    SetInventoryHint(FText::Format(
        LOCTEXT("InventorySplitTarget", "请选择空格，将当前堆叠的一半（{0}个）拆出。"),
        FText::AsNumber(FMath::Max(1, Source->Quantity / 2))));
}

void UDivineBeastsInventoryScreen::HandleMergeClicked()
{
    if (!CanModifyInventory() || SelectedItemInstanceId.IsEmpty()) { return; }
    TargetAction = ETargetAction::Merge;
    SetInventoryHint(LOCTEXT("InventoryMergeTarget", "请选择相同物品的另一堆，数量不能超过上限。"));
}

void UDivineBeastsInventoryScreen::HandleBindQuickbarClicked()
{
    if (!CanModifyInventory() || SelectedItemInstanceId.IsEmpty()) { return; }
    const FGuid Accepted = RequestSetInventoryQuickbar(SelectedQuickbarIndex, SelectedItemInstanceId);
    SetInventoryHint(Accepted.IsValid()
        ? LOCTEXT("InventoryBindRequested", "已提交快捷栏绑定，等待服务器确认。")
        : LOCTEXT("InventoryBindRejected", "暂时无法将此物品放入快捷栏。"));
}

void UDivineBeastsInventoryScreen::HandleClearQuickbarClicked()
{
    if (!CanModifyInventory()) { return; }
    const FGuid Accepted = RequestClearInventoryQuickbar(SelectedQuickbarIndex);
    SetInventoryHint(Accepted.IsValid()
        ? LOCTEXT("InventoryUnbindRequested", "已请求清除快捷栏引用，物品仍保留在背包。")
        : LOCTEXT("InventoryUnbindRejected", "清除快捷栏请求未被受理。"));
}

void UDivineBeastsInventoryScreen::BindUIEvents()
{
    Super::BindUIEvents();
    BindInventoryVisualControls();

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
    UnbindInventoryVisualControls();
    if (IsValid(InventorySubsystem) &&
        InventoryChangedHandle.IsValid())
    {
        InventorySubsystem->OnChanged.Remove(InventoryChangedHandle);
        InventoryChangedHandle.Reset();
    }
    InventorySubsystem = nullptr;

    Super::UnbindUIEvents();
}

void UDivineBeastsInventoryScreen::NativeDestruct()
{
    UnbindInventoryVisualControls();
    Super::NativeDestruct();
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
        // 无所属账号时必须同时清除前一个玩家的选择及视觉投影。
        SelectedItemInstanceId.Reset();
        TargetAction = ETargetAction::None;
        ActionHint = FText::GetEmpty();
        RefreshInventoryVisuals();
        BP_OnInventoryViewChanged(
            0, EGamePlatformInventoryClientState::Uninitialized, false);
        return;
    }

    if (Subsystem->GetInventoryRevision() <= 0 ||
        (!SelectedItemInstanceId.IsEmpty() &&
         !Subsystem->FindItem(SelectedItemInstanceId)))
    {
        SelectedItemInstanceId.Reset();
        TargetAction = ETargetAction::None;
        ActionHint = FText::GetEmpty();
    }
    RefreshInventoryVisuals();

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
