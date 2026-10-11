#pragma once

#include "Components/GamePlatformResourceBarWidget.h"
#include "Contracts/DivineBeastsPlayerStatusUIContracts.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "DivineBeastsPlayerStatusPanel.generated.h"

class UDivineBeastsPlayerStatusViewModel;
class APawn;

/**
 * UDivineBeastsPlayerStatusPanel（神兽联盟玩家状态面板）。
 *
 * 组合平台ResourceBar（资源条）视觉原子，只显示生命与气势；护盾由状态效果图标显示。
 * 页面/HUD的领域ViewModel在事实变化时调用 ApplyStatus；Panel自身不轮询Gameplay对象。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPlayerStatusPanel
    : public UDivineBeastsCombatPanelBase
{
    GENERATED_BODY()

public:
    /** 根布局在控制器/Pawn换代事件中重绑当前ASC；空来源清空旧生命/气势，不执行玩法写入。 */
    void RefreshStatusSourceFromOwningPawn();
    /** 绑定事件驱动状态 ViewModel；Panel 不直接读取 ASC。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    void BindStatusViewModel(UDivineBeastsPlayerStatusViewModel* InViewModel);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    void ClearStatusViewModel();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    void ApplyStatus(const FDivineBeastsPlayerStatusViewData& InStatus);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Combat")
    FDivineBeastsPlayerStatusViewData GetStatus() const { return Status; }

protected:
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> HealthBar = nullptr;

    /** 历史蓝图ShieldBar（盾资源条）绑定仍保留以避免资源重载失败，启动时强制隐藏。
     * 删除蓝图树中的此控件必须由Monolith MCP执行，不能在文本工具中篡改uasset。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> ShieldBar = nullptr;

    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> MomentumBar = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Combat", meta=(DisplayName="玩家状态已变化"))
    void BP_OnPlayerStatusChanged(FDivineBeastsPlayerStatusViewData NewStatus);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    void HandleStatusChanged(const FDivineBeastsPlayerStatusViewData& NewStatus);

private:
    UPROPERTY(Transient)
    FDivineBeastsPlayerStatusViewData Status;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsPlayerStatusViewModel> StatusViewModel = nullptr;

    /** 此面板在本地按Pawn实例持有唯一UI ViewModel，不创建Gameplay侧第二份生命或气势。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsPlayerStatusViewModel> OwnedStatusViewModel = nullptr;

    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    /** 当前控制器委托的真实来源；失活和换代解除旧监听，不用已更新的拥有者查找旧控制器。 */
    TWeakObjectPtr<APlayerController> BoundPawnController;
    /** 所拥有只读视图模型的属性事件句柄。 */
    FDelegateHandle StatusChangedHandle;
};
