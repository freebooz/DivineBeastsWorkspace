#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "GamePlatformUITypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformUILayer : uint8
{
    HUD,
    Screen,
    Modal,
    System,
    Notification,
    Loading,
    Debug
};

UENUM(BlueprintType)
enum class EGamePlatformUIInputMode : uint8
{
    GameOnly,
    UIOnly,
    GameAndUI
};

UENUM(BlueprintType)
enum class EGamePlatformUIPausePolicy : uint8
{
    Never,
    StandaloneOnly
};

UENUM(BlueprintType)
enum class EGamePlatformUITransition : uint8
{
    None,
    Default,
    Instant
};

/** EGamePlatformUIPlatformClass（游戏平台 UI 运行平台分类）。 */
UENUM(BlueprintType)
enum class EGamePlatformUIPlatformClass : uint8
{
    /** 桌面、主机或不依赖触摸屏的常规客户端平台。 */
    Desktop,

    /** Android、iOS 等以触控和安全区域适配为核心的移动平台。 */
    Mobile
};

/** EGamePlatformUILayoutClass（游戏平台响应式布局等级）。 */
UENUM(BlueprintType)
enum class EGamePlatformUILayoutClass : uint8
{
    /** 紧凑布局：主要用于手机、窄窗口和较小逻辑宽度。 */
    Compact,

    /** 标准布局：主要用于平板、掌机或中等逻辑宽度窗口。 */
    Regular,

    /** 宽屏布局：主要用于 PC、主机和大尺寸横屏。 */
    Wide
};

/** EGamePlatformUIInputDeviceClass（游戏平台 UI 输入设备分类）。 */
UENUM(BlueprintType)
enum class EGamePlatformUIInputDeviceClass : uint8
{
    /** 键盘与鼠标。 */
    KeyboardMouse,

    /** 游戏手柄。 */
    Gamepad,

    /** 触摸输入。 */
    Touch
};

/** EGamePlatformUIOrientation（游戏平台 UI 屏幕方向）。 */
UENUM(BlueprintType)
enum class EGamePlatformUIOrientation : uint8
{
    /** 横屏或等宽高视口。 */
    Landscape,

    /** 竖屏视口。 */
    Portrait
};

/**
 * FGamePlatformUIAdaptiveContext（游戏平台 UI 自适应上下文）。
 *
 * 该结构只保存轻量值类型，由 LocalPlayer 级自适应子系统低频更新。
 * Widget 通过事件获取变化，不应在 Tick 中重复计算视口、DPI 或输入设备。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIAdaptiveContext
{
    GENERATED_BODY()

    /** 当前运行平台的大类。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    EGamePlatformUIPlatformClass PlatformClass =
        EGamePlatformUIPlatformClass::Desktop;

    /** 当前 CommonInput 识别的主要输入设备。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    EGamePlatformUIInputDeviceClass InputDeviceClass =
        EGamePlatformUIInputDeviceClass::KeyboardMouse;

    /** 当前响应式布局等级。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    EGamePlatformUILayoutClass LayoutClass =
        EGamePlatformUILayoutClass::Regular;

    /** 当前屏幕方向。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    EGamePlatformUIOrientation Orientation =
        EGamePlatformUIOrientation::Landscape;

    /** 当前 LocalPlayer 所在视口像素尺寸。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    FVector2D ViewportSize = FVector2D::ZeroVector;

    /** 当前项目 UISettings 根据视口计算得到的 DPI 缩放。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    float DPIScale = 1.0f;

    /** 当前视口宽高比；高度无效时保持默认 16:9。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    float AspectRatio = 16.0f / 9.0f;

    /**
     * 当前平台安全区域边距，单位为 Slate/视口像素。
     * 移动端用于避让刘海、挖孔、圆角和系统手势区域；桌面通常为零。
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    FMargin SafeZonePadding;

    /** 当前设备是否应提供触控友好布局和触控目标尺寸。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Adaptive")
    bool bTouchCapable = false;

    /**
     * 判断两个快照是否在 UI 语义上等价。
     * 浮点值使用容差比较，避免窗口轻微浮点抖动产生无意义广播。
     */
    bool IsEquivalentTo(const FGamePlatformUIAdaptiveContext& Other) const
    {
        return PlatformClass == Other.PlatformClass &&
            InputDeviceClass == Other.InputDeviceClass &&
            LayoutClass == Other.LayoutClass &&
            Orientation == Other.Orientation &&
            ViewportSize.Equals(Other.ViewportSize, 0.5f) &&
            FMath::IsNearlyEqual(DPIScale, Other.DPIScale, 0.001f) &&
            FMath::IsNearlyEqual(AspectRatio, Other.AspectRatio, 0.001f) &&
            SafeZonePadding == Other.SafeZonePadding &&
            bTouchCapable == Other.bTouchCapable;
    }
};

/** 跨游戏可访问性偏好钩子；视觉资产由上层Blueprint/Style消费。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIAccessibilityPreferences
{
    GENERATED_BODY()

    /** 文本缩放倍率；Manager会拒绝不可读的过小值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    float TextScale = 1.0f;

    /** 触控目标放大倍率；可访问性设置不允许把目标缩得比默认更小。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    float TouchTargetScale = 1.0f;

    /** 减少非必要界面动画；默认Transition会降级为Instant。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bReducedMotion = false;

    /** 请求高对比度样式；具体样式资源由客户端组合层提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bPreferHighContrast = false;

    /** 状态表达需要图标/文本等非颜色线索，不能只依赖颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bRequireNonColorStatusCues = true;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIAsyncRequest
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    FGuid RequestId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    FName ScreenId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    int32 Generation = 0;

    bool IsValid() const { return RequestId.IsValid() && !ScreenId.IsNone(); }
};
