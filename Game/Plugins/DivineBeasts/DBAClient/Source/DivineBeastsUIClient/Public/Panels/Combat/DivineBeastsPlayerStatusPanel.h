#pragma once

#include "Components/GamePlatformResourceBarWidget.h"
#include "Contracts/DivineBeastsPlayerStatusUIContracts.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "DivineBeastsPlayerStatusPanel.generated.h"

class UDivineBeastsPlayerStatusViewModel;

/**
 * UDivineBeastsPlayerStatusPanel（神兽联盟玩家状态面板）。
 *
 * 组合平台 ResourceBar（资源条）视觉原子，统一展示生命、护盾和气势。
 * 页面/HUD的领域ViewModel在事实变化时调用 ApplyStatus；Panel自身不轮询Gameplay对象。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPlayerStatusPanel
    : public UDivineBeastsCombatPanelBase
{
    GENERATED_BODY()

public:
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

    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> ShieldBar = nullptr;

    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> MomentumBar = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Combat", meta=(DisplayName="玩家状态已变化"))
    void BP_OnPlayerStatusChanged(FDivineBeastsPlayerStatusViewData NewStatus);

    virtual void NativeDestruct() override;

    void HandleStatusChanged(const FDivineBeastsPlayerStatusViewData& NewStatus);

private:
    UPROPERTY(Transient)
    FDivineBeastsPlayerStatusViewData Status;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsPlayerStatusViewModel> StatusViewModel = nullptr;

    FDelegateHandle StatusChangedHandle;
};
