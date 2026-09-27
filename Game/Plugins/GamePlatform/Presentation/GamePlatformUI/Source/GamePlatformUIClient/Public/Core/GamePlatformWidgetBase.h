#pragma once

#include "CommonUserWidget.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformWidgetBase.generated.h"

class UGamePlatformUIAdaptiveSubsystem;
class UGamePlatformViewModelBase;

/**
 * UGamePlatformWidgetBase（游戏平台普通用户界面基类）。
 *
 * 职责：
 * 1. 作为所有“不进入 CommonUI 激活栈”的平台 UI 控件统一父类。
 * 2. 统一管理 ViewModel（视图模型）事件绑定与解绑，避免业务控件重复实现生命周期代码。
 * 3. 统一接入 PC / 移动端 Adaptive Context（自适应上下文）变化事件。
 * 4. 为 Blueprint（蓝图）和 C++ 子类提供事件驱动刷新钩子。
 *
 * 性能约束：
 * - 本类不启用 Tick，不允许通过逐帧轮询刷新业务状态。
 * - 委托仅在 NativeConstruct 到 NativeDestruct 期间绑定。
 * - 自适应上下文只在真实变化时广播，不产生每帧分配。
 *
 * 使用约束：
 * - HUD、Panel、Overlay、Notification、Component 等普通界面必须从本类或其分类子类继承。
 * - 子类扩展 BindUIEvents / UnbindUIEvents 时不得重复绑定平台 ViewModel 与自适应事件。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformWidgetBase : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    /**
     * 设置当前控件使用的 ViewModel。
     * 如果控件已经构造完成，会安全解绑旧 ViewModel、绑定新 ViewModel，并立即刷新初始状态。
     */
    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void InitializeWidgetViewModel(UGamePlatformViewModelBase* InViewModel);

    /** 返回当前控件绑定的 ViewModel；未绑定时返回 nullptr。 */
    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    UGamePlatformViewModelBase* GetViewModel() const { return ViewModel; }

    /** 返回当前 LocalPlayer（本地玩家）的 UI 自适应上下文快照。 */
    UFUNCTION(BlueprintPure, Category="UI|Adaptive")
    FGamePlatformUIAdaptiveContext GetAdaptiveContext() const;

protected:
    /** UUserWidget 构造完成后统一绑定平台事件和业务扩展事件。 */
    virtual void NativeConstruct() override;

    /** UUserWidget 销毁前统一解绑所有事件，避免跨页面或跨世界残留回调。 */
    virtual void NativeDestruct() override;

    /** 子类用于绑定自身业务事件；平台事件已经由基类绑定。 */
    virtual void BindUIEvents() {}

    /** 子类用于解绑自身业务事件；必须与 BindUIEvents 成对。 */
    virtual void UnbindUIEvents() {}

    /** 子类用于根据当前快照执行一次初始刷新，不应主动轮询业务系统。 */
    virtual void RefreshInitialState() {}

    /** ViewModel 状态变化的 C++ 扩展点；默认不做额外处理。 */
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) {}

    /** PC / 移动端自适应上下文变化的 C++ 扩展点；默认不做额外处理。 */
    virtual void OnAdaptiveContextChanged(const FGamePlatformUIAdaptiveContext& Context) {}

    /** 蓝图扩展点：ViewModel 状态变化后调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|ViewModel", meta=(DisplayName="视图状态已变化"))
    void BP_OnViewModelStateChanged(int32 Revision, int32 PageGeneration);

    /** 蓝图扩展点：PC / 移动端自适应上下文变化后调用。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Adaptive", meta=(DisplayName="界面自适应上下文已变化"))
    void BP_OnAdaptiveContextChanged(FGamePlatformUIAdaptiveContext Context);

private:
    /** 处理平台 ViewModel 的统一状态变化广播。 */
    UFUNCTION()
    void HandleViewModelStateChanged(int32 Revision, int32 PageGeneration);

    /** 处理 LocalPlayer 自适应上下文变化广播。 */
    UFUNCTION()
    void HandleAdaptiveContextChanged(FGamePlatformUIAdaptiveContext Context);

    /** 绑定平台拥有的 ViewModel 与 Adaptive 事件。 */
    void BindPlatformEvents();

    /** 解绑平台拥有的 ViewModel 与 Adaptive 事件。 */
    void UnbindPlatformEvents();

    /** 当前控件使用的 ViewModel；Transient（瞬态）避免进入资产序列化。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformViewModelBase> ViewModel = nullptr;

    /** 当前绑定的自适应子系统；仅用于生命周期内快速解绑，避免重复查找。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIAdaptiveSubsystem> AdaptiveSubsystem = nullptr;

    /** 防止 NativeConstruct 重入导致重复绑定委托。 */
    bool bPlatformEventsBound = false;
};
