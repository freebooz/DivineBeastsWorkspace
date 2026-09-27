#pragma once

#include "CommonInputBaseTypes.h"
#include "CoreMinimal.h"
#include "GamePlatformUITypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformUIAdaptiveSubsystem.generated.h"

class FViewport;
class UCommonInputSubsystem;

/**
 * 自适应上下文变化事件。
 * 事件仅在平台类型、输入设备、视口尺寸、DPI、方向或布局等级真实变化时广播。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUIAdaptiveContextChanged,
    FGamePlatformUIAdaptiveContext,
    Context);

/**
 * UGamePlatformUIAdaptiveSubsystem（游戏平台用户界面自适应子系统）。
 *
 * 作用域：
 * - 每个 LocalPlayer（本地玩家）独立实例，避免分屏或多本地玩家共享错误状态。
 *
 * 职责：
 * 1. 监听 CommonInput 输入方式变化。
 * 2. 监听视口尺寸变化。
 * 3. 计算 Desktop / Mobile、Compact / Regular / Wide、横竖屏和 DPI 等上下文。
 * 4. 仅在上下文变化时广播事件，为所有 UI 提供统一 PC / 移动端适配入口。
 *
 * 性能策略：
 * - 不使用 Tick。
 * - 上下文只在输入方式或视口真实变化时重算。
 * - 广播前比较新旧快照，未变化时不触发下游刷新。
 */
UCLASS()
class GAMEPLATFORMUICLIENT_API UGamePlatformUIAdaptiveSubsystem
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 初始化输入和视口事件监听。 */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** 关闭时解绑事件，禁止残留跨 LocalPlayer 回调。 */
    virtual void Deinitialize() override;

    /** 返回当前自适应上下文快照。 */
    UFUNCTION(BlueprintPure, Category="UI|Adaptive")
    FGamePlatformUIAdaptiveContext GetAdaptiveContext() const { return AdaptiveContext; }

    /**
     * 手动请求重新计算上下文。
     * 主要供分辨率设置应用完成、测试或平台特定事件调用，不应每帧调用。
     */
    UFUNCTION(BlueprintCallable, Category="UI|Adaptive")
    void RefreshAdaptiveContext();

    /** 自适应上下文真实变化时广播。 */
    UPROPERTY(BlueprintAssignable, Category="UI|Adaptive")
    FGamePlatformUIAdaptiveContextChanged OnAdaptiveContextChanged;

private:
    /**
     * CommonInput 原生输入方式变化回调。
     * 使用 OnInputMethodChangedNative（原生事件）避免依赖 CommonInput 的私有蓝图委托。
     */
    void HandleInputMethodChanged(ECommonInputType NewInputType);

    /** Unreal 视口尺寸变化回调。 */
    void HandleViewportResized(FViewport* Viewport, uint32 Unused);

    /** 根据 CommonInput 类型转换为平台稳定输入分类。 */
    static EGamePlatformUIInputDeviceClass ResolveInputDeviceClass(
        ECommonInputType InputType);

    /** 根据当前运行平台判断 Desktop / Mobile。 */
    static EGamePlatformUIPlatformClass ResolvePlatformClass();

    /** 根据逻辑宽度选择 Compact / Regular / Wide。 */
    static EGamePlatformUILayoutClass ResolveLayoutClass(
        const FVector2D& ViewportSize,
        float DPIScale);

    /** 读取当前 LocalPlayer 的视口、DPI 与输入信息并构建新快照。 */
    FGamePlatformUIAdaptiveContext BuildAdaptiveContext() const;

    /** CommonInput 本地玩家子系统缓存；用于 O(1) 读取当前输入方式和解绑。 */
    UPROPERTY(Transient)
    TObjectPtr<UCommonInputSubsystem> CommonInputSubsystem = nullptr;

    /** 当前已发布的自适应上下文。 */
    UPROPERTY(Transient)
    FGamePlatformUIAdaptiveContext AdaptiveContext;

    /** CommonInput 原生输入方式变化事件句柄；用于 O(1) 精确解绑。 */
    FDelegateHandle InputMethodChangedHandle;

    /** FViewport 静态事件绑定句柄。 */
    FDelegateHandle ViewportResizedHandle;
};
