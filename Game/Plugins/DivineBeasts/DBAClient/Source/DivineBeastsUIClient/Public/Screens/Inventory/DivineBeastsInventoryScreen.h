#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "Services/GamePlatformInventoryClientSubsystem.h"
#include "DivineBeastsInventoryScreen.generated.h"


class UButton;
class UDivineBeastsInventoryScreen;

/**
 * UDivineBeastsInventoryButtonProxy（背包格点击转接对象）。
 * 一格一实例，避免在UMG无参数OnClicked回调中猜测当前焦点/鼠标控件。
 * 只负责把稳定格序号传回当前页面，不存储账号、物品或库存快照。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsInventoryButtonProxy final : public UObject
{
    GENERATED_BODY()

public:
    /** 绑定一个Monolith创建的命名按钮，页面失活时必须精确Unbind。 */
    void Bind(UDivineBeastsInventoryScreen* Screen, UButton* Button, int32 Index, bool bQuickbar);
    void Unbind();

    UFUNCTION()
    void HandleButtonClicked();

private:
    TWeakObjectPtr<UDivineBeastsInventoryScreen> ScreenOwner;
    TWeakObjectPtr<UButton> ButtonOwner;
    int32 LocalIndex = INDEX_NONE;
    bool bIsQuickbar = false;
};

/**
 * UDivineBeastsInventoryScreen（神兽联盟背包页面基类）。
 *
 * 数据所有权：
 * - 直接消费 GamePlatformInventoryClient（游戏平台背包客户端）的权威快照缓存和中立ViewModel。
 * - 不在神兽联盟UI层复制第二套背包状态、排序缓存或Pending Operation（待处理操作）。
 *
 * 性能：
 * - 页面激活期间订阅一次 OnChanged 原生事件；失活时精确解绑。
 * - C++热路径读取 GetViewModelsView() 缓存；仅Blueprint值返回时才复制数组。
 * - 不使用Tick，不每帧排序背包。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsInventoryScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    UDivineBeastsInventoryScreen()
    {
        UIDomain = EDivineBeastsUIDomain::Inventory;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    EGamePlatformInventoryClientState GetInventoryState() const;

    /** 返回平台背包最近一次稳定错误，供项目蓝图显示提示；不解析服务端自然语言消息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    EGamePlatformInventoryError GetInventoryLastError() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    int64 GetInventoryRevision() const;
    
    /** 服务端状态的中文文本，供蓝图无Tick显示加载、提交及对账状态。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    FText GetInventoryStateText() const;

    /** 最近一次稳定错误的中文提示；不包含后端自然语言或账号信息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    FText GetInventoryErrorText() const;

    /** 只有Ready且无未决写入时允许玩家提交新物品移动类命令。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    bool CanModifyInventory() const;


    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    bool HasPendingInventoryOperation() const;

    /** Blueprint读取接口；返回平台已缓存的派生ViewModel副本。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    TArray<FGamePlatformInventoryItemViewModel> GetInventoryItems() const;

    /** C++高频读取接口，不复制数组。 */
    const TArray<FGamePlatformInventoryItemViewModel>&
    GetInventoryItemsView() const;

    /** 返回后端当前容器清单；UI可根据Capacity创建虚拟化网格，不硬编码64格。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    TArray<FGamePlatformInventoryContainerSnapshot> GetInventoryContainers() const;

    /** 未认证、未加载或容器不存在时容量为0；不是客户端可修改的背包上限。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    int32 GetInventoryContainerCapacity(FName ContainerId) const;

    /** 单槽投影：true表示有效已解锁位置，bOccupied=false是正常空格。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    bool TryGetInventorySlot(
        FName ContainerId, int32 SlotIndex,
        FGamePlatformInventorySlotViewModel& OutSlot) const;

    /** 按需最多返回128格，供蓝图Grid/ListView事件刷新，绝不在Tick中遍历。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    TArray<FGamePlatformInventorySlotViewModel> GetInventorySlotPage(
        FName ContainerId, int32 StartSlotIndex, int32 MaxSlotCount) const;

    /** 快捷栏固定12格，与物品栏共享实例身份，不创建额外的游戏库存。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    TArray<FGamePlatformInventoryQuickbarViewModel> GetInventoryQuickbarSlots() const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    bool RefreshInventory();

    /** 请求结果未知时仅查询原OperationId，不能重复发奖或盲目新建写操作。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    bool RetryPendingInventoryOperation();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestMoveItem(
        const FString& ItemInstanceId,
        FName TargetContainerId,
        int32 TargetSlotIndex);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestSplitItem(
        const FString& SourceItemInstanceId,
        int32 SplitQuantity,
        FName TargetContainerId,
        int32 TargetSlotIndex);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestMergeItem(
        const FString& SourceItemInstanceId,
        const FString& TargetItemInstanceId);
    
    /**
     * 玩家将源物品拖放到目标容器槽位时的唯一命令入口。
     * 同定义且可完整合并时提交Merge，否则由权威服务执行Move/Swap；
     * 同位置、非法目标或同定义但超堆叠上限时拒绝而不改变本地数据。
     * bPreferMerge为false时总是尝试移动/交换；成功Guid仅表示异步命令受理。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestDropInventoryItem(
        const FString& SourceItemInstanceId,
        FName TargetContainerId,
        int32 TargetSlotIndex,
        bool bPreferMerge = true);


    /** 绑定快捷格到背包已有实例；返回Guid仅代表请求受理，权威状态看事件快照。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestSetInventoryQuickbar(int32 SlotIndex, const FString& ItemInstanceId);

    /** 清除快捷格引用，不会消耗/丢弃真实物品。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    FGuid RequestClearInventoryQuickbar(int32 SlotIndex);

protected:
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    /** Widget被销毁时再次精确释放点击委托，避免跨世界残留旧事件。 */
    virtual void NativeDestruct() override;

    /** 背包平台状态变化后通知视觉层执行局部刷新。 */
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Inventory", meta=(DisplayName="背包视图已变化"))
    void BP_OnInventoryViewChanged(
        int64 InventoryRevision,
        EGamePlatformInventoryClientState State,
        bool bHasPendingOperation);

private:
    void HandleInventoryChanged();
    UGamePlatformInventoryClientSubsystem* ResolveInventorySubsystem() const;


    // 下面的函数只消费Monolith Widget资产中真实存在的命名控件；
    // 不通过C++构建另一棵视觉树，也不在Tick查询背包网络状态。
    void BindInventoryVisualControls();
    void UnbindInventoryVisualControls();
    void RefreshInventoryVisuals();
    void HandleInventoryCellClicked(int32 LocalIndex, bool bQuickbar);
    void SetInventoryHint(const FText& Hint);
    void BindNamedActionButtons();
    void UnbindNamedActionButtons();

    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandlePrevPageClicked();
    UFUNCTION() void HandleNextPageClicked();
    UFUNCTION() void HandleRefreshClicked();
    UFUNCTION() void HandleRetryClicked();
    UFUNCTION() void HandleMoveClicked();
    UFUNCTION() void HandleSplitClicked();
    UFUNCTION() void HandleMergeClicked();
    UFUNCTION() void HandleBindQuickbarClicked();
    UFUNCTION() void HandleClearQuickbarClicked();

    /** 操作意图是UI局部选择状态；下一次有效服务器操作结果仍以Snapshot为准。 */
    enum class ETargetAction : uint8 { None, Move, Split, Merge };
    ETargetAction TargetAction = ETargetAction::None;
    FName SelectedContainerId = TEXT("main");
    FString SelectedItemInstanceId;
    FText ActionHint;
    int32 PageIndex = 0;
    int32 SelectedQuickbarIndex = 0;
    static constexpr int32 SlotsPerPage = 32;
    bool bVisualControlsBound = false;
    bool bRefreshingVisuals = false;

    /** 每次激活持有32格+12快捷格的按钮代理；失活解绑、取消代理引用。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UDivineBeastsInventoryButtonProxy>> SlotButtonProxies;
    friend class UDivineBeastsInventoryButtonProxy;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformInventoryClientSubsystem> InventorySubsystem = nullptr;

    FDelegateHandle InventoryChangedHandle;
};
