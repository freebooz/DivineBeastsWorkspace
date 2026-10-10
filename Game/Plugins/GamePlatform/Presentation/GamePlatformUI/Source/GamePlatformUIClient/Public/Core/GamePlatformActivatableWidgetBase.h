// 本文件属于GamePlatform平台层客户端UI，约束可激活页与VM事件/页面代次所有权；不持有业务权威。
// 中文参数/失败/重入/生命周期见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CommonActivatableWidget.h"
#include "GamePlatformUITypes.h"
#include "Styling/GamePlatformUIThemeTypes.h"
#include "GamePlatformActivatableWidgetBase.generated.h"

class UGamePlatformUIThemeBinding;
class UGamePlatformUIAdaptiveSubsystem;
class UGamePlatformViewModelBase;

/**
 * UGamePlatformActivatableWidgetBase（游戏平台可激活用户界面基类）。
 *
 * 职责：
 * 1. 为 Screen、Menu、Modal、Loading 等 CommonUI 可激活界面提供统一生命周期。
 * 2. 在页面激活时启动 ViewModel 页面代次，并绑定事件；失活时立即解绑并结束代次。
 * 3. 接收 PC / 移动端自适应变化事件，避免子页面自行轮询分辨率或输入设备。
 *
 * 性能约束：
 * - 不启用 Tick。
 * - 仅在激活期间持有动态委托。
 * - 页面失活后所有平台事件必须解绑，防止迟到回调刷新不可见页面。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformActivatableWidgetBase
    : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 默认空数组保持旧页面不变；名称只引用自身WidgetTree，不更改按钮类型或业务命令。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Theme")
    TArray<FGamePlatformUIWidgetStyleBinding> ThemeBindings;

    /**
     * 设置当前可激活界面的 ViewModel。
     * 激活状态下替换 ViewModel 时，会结束旧页面代次并启动新页面代次。
     */
    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void InitializeActivatableViewModel(UGamePlatformViewModelBase* InViewModel);

    /** 返回当前页面绑定的 ViewModel。 */
    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    UGamePlatformViewModelBase* GetPlatformViewModel() const { return ViewModel; }

    /** 返回当前 LocalPlayer 的自适应上下文。 */
    UFUNCTION(BlueprintPure, Category="UI|Adaptive")
    FGamePlatformUIAdaptiveContext GetAdaptiveContext() const;

protected:
    /** 页面激活时启动 ViewModel 生命周期并绑定事件。 */
    virtual void NativeOnActivated() override;

    /** 页面失活时先解绑事件，再结束 ViewModel 页面生命周期。 */
    virtual void NativeOnDeactivated() override;

    /** 子类业务事件绑定扩展点。 */
    virtual void BindUIEvents() {}

    /** 子类业务事件解绑扩展点。 */
    virtual void UnbindUIEvents() {}

    /** 子类初始状态刷新扩展点。 */
    virtual void RefreshInitialState() {}

    /** ViewModel 状态变化的 C++ 扩展点。 */
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) {}

    /** 自适应上下文变化的 C++ 扩展点。 */
    virtual void OnAdaptiveContextChanged(const FGamePlatformUIAdaptiveContext& Context) {}

    /** 蓝图扩展点：ViewModel 状态变化。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|ViewModel", meta=(DisplayName="视图状态已变化"))
    void BP_OnViewModelStateChanged(int32 Revision, int32 PageGeneration);

    /** 蓝图扩展点：PC / 移动端自适应上下文变化。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Adaptive", meta=(DisplayName="界面自适应上下文已变化"))
    void BP_OnAdaptiveContextChanged(FGamePlatformUIAdaptiveContext Context);

private:
    /** 激活期间接收主题事件，失活保留实际绘制类引用，重新激活使用最新主题。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIThemeBinding> ThemeBinding = nullptr;

    /** 接收 ViewModel 状态变化，并转发到 C++ / Blueprint 扩展点。 */
    UFUNCTION()
    void HandleViewModelStateChanged(int32 Revision, int32 PageGeneration);

    /** 接收 Adaptive Context 变化，并转发到 C++ / Blueprint 扩展点。 */
    UFUNCTION()
    void HandleAdaptiveContextChanged(FGamePlatformUIAdaptiveContext Context);

    /** 绑定激活期间的平台事件。 */
    void BindPlatformEvents();

    /** 解绑激活期间的平台事件。 */
    void UnbindPlatformEvents();

    /** 当前页面 ViewModel。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformViewModelBase> ViewModel = nullptr;

    /** 当前 LocalPlayer 的 UI 自适应子系统。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIAdaptiveSubsystem> AdaptiveSubsystem = nullptr;

    /** 防止同一激活周期重复绑定委托。 */
    bool bPlatformEventsBound = false;
    /** 激活/失活每次递增；外部钩子返回后必须仍属于同一可见周期。 */
    uint64 ActivationGeneration = 0;
    /** VM更换每次递增；嵌套更换优先，旧栈返回不得覆盖或重新绑定。 */
    uint64 ViewModelBindingGeneration = 0;
};
