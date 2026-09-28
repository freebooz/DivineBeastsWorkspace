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
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    EGamePlatformInventoryClientState GetInventoryState() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    int64 GetInventoryRevision() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    bool HasPendingInventoryOperation() const;

    /** Blueprint读取接口；返回平台已缓存的派生ViewModel副本。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Inventory")
    TArray<FGamePlatformInventoryItemViewModel> GetInventoryItems() const;

    /** C++高频读取接口，不复制数组。 */
    const TArray<FGamePlatformInventoryItemViewModel>&
    GetInventoryItemsView() const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Inventory")
    bool RefreshInventory();

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
