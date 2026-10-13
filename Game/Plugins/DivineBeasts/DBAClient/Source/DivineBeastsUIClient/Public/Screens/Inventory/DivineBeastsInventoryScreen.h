#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "Services/GamePlatformInventoryClientSubsystem.h"
#include "DivineBeastsInventoryScreen.generated.h"

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

    /** 背包平台状态变化后通知视觉层执行局部刷新。 */
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Inventory", meta=(DisplayName="背包视图已变化"))
    void BP_OnInventoryViewChanged(
        int64 InventoryRevision,
        EGamePlatformInventoryClientState State,
        bool bHasPendingOperation);

private:
    void HandleInventoryChanged();
    UGamePlatformInventoryClientSubsystem* ResolveInventorySubsystem() const;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformInventoryClientSubsystem> InventorySubsystem = nullptr;

    FDelegateHandle InventoryChangedHandle;
};
