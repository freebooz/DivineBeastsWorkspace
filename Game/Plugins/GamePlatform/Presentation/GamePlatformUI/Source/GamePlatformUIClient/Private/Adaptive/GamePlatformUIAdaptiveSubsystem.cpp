#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"

#include "CommonInputSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"

namespace
{
    /** 小于该逻辑宽度时采用紧凑布局，主要覆盖手机与窄窗口。 */
    constexpr float CompactLogicalWidth = 900.0f;

    /** 大于等于该逻辑宽度时采用宽屏布局，主要覆盖桌面和大屏。 */
    constexpr float WideLogicalWidth = 1400.0f;
}

void UGamePlatformUIAdaptiveSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        CommonInputSubsystem =
            LocalPlayer->GetSubsystem<UCommonInputSubsystem>();
    }

    if (IsValid(CommonInputSubsystem))
    {
        // 使用 CommonInput 对外公开的原生事件。
        // 相比动态蓝图委托，原生委托无需反射调用，输入设备切换路径更轻量。
        InputMethodChangedHandle =
            CommonInputSubsystem->OnInputMethodChangedNative.AddUObject(
                this,
                &UGamePlatformUIAdaptiveSubsystem::HandleInputMethodChanged);
    }

    // 视口尺寸变化属于低频事件；使用引擎事件避免每帧检查分辨率。
    ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(
        this,
        &UGamePlatformUIAdaptiveSubsystem::HandleViewportResized);

    RefreshAdaptiveContext();
}

void UGamePlatformUIAdaptiveSubsystem::Deinitialize()
{
    if (IsValid(CommonInputSubsystem) &&
        InputMethodChangedHandle.IsValid())
    {
        CommonInputSubsystem->OnInputMethodChangedNative.Remove(
            InputMethodChangedHandle);
        InputMethodChangedHandle.Reset();
    }
    CommonInputSubsystem = nullptr;

    if (ViewportResizedHandle.IsValid())
    {
        FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
        ViewportResizedHandle.Reset();
    }

    Super::Deinitialize();
}

void UGamePlatformUIAdaptiveSubsystem::RefreshAdaptiveContext()
{
    const FGamePlatformUIAdaptiveContext NewContext = BuildAdaptiveContext();
    if (AdaptiveContext.IsEquivalentTo(NewContext))
    {
        return;
    }

    AdaptiveContext = NewContext;
    OnAdaptiveContextChanged.Broadcast(AdaptiveContext);
}

void UGamePlatformUIAdaptiveSubsystem::HandleInputMethodChanged(
    ECommonInputType NewInputType)
{
    // NewInputType 已经由 CommonInput 更新完成。
    // 保持所有上下文计算从 BuildAdaptiveContext 进入，避免出现两套状态真源。
    RefreshAdaptiveContext();
}

void UGamePlatformUIAdaptiveSubsystem::HandleViewportResized(
    FViewport* Viewport,
    uint32 Unused)
{
    // FViewport 事件为进程级广播。每个 LocalPlayer 仅重算自己的轻量快照，
    // RefreshAdaptiveContext 会进一步过滤无变化结果，避免无效 UI 刷新。
    RefreshAdaptiveContext();
}

EGamePlatformUIInputDeviceClass
UGamePlatformUIAdaptiveSubsystem::ResolveInputDeviceClass(
    ECommonInputType InputType)
{
    switch (InputType)
    {
    case ECommonInputType::Gamepad:
        return EGamePlatformUIInputDeviceClass::Gamepad;
    case ECommonInputType::Touch:
        return EGamePlatformUIInputDeviceClass::Touch;
    case ECommonInputType::MouseAndKeyboard:
    default:
        return EGamePlatformUIInputDeviceClass::KeyboardMouse;
    }
}

EGamePlatformUIPlatformClass
UGamePlatformUIAdaptiveSubsystem::ResolvePlatformClass()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return EGamePlatformUIPlatformClass::Mobile;
#else
    return EGamePlatformUIPlatformClass::Desktop;
#endif
}

EGamePlatformUILayoutClass
UGamePlatformUIAdaptiveSubsystem::ResolveLayoutClass(
    const FVector2D& ViewportSize,
    float DPIScale)
{
    const float SafeScale = FMath::Max(DPIScale, KINDA_SMALL_NUMBER);
    const float LogicalWidth = ViewportSize.X / SafeScale;

    if (LogicalWidth < CompactLogicalWidth)
    {
        return EGamePlatformUILayoutClass::Compact;
    }
    if (LogicalWidth >= WideLogicalWidth)
    {
        return EGamePlatformUILayoutClass::Wide;
    }
    return EGamePlatformUILayoutClass::Regular;
}

FGamePlatformUIAdaptiveContext
UGamePlatformUIAdaptiveSubsystem::BuildAdaptiveContext() const
{
    FGamePlatformUIAdaptiveContext Result;
    Result.PlatformClass = ResolvePlatformClass();

    if (IsValid(CommonInputSubsystem))
    {
        Result.InputDeviceClass = ResolveInputDeviceClass(
            CommonInputSubsystem->GetCurrentInputType());
    }

    if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (const UGameViewportClient* ViewportClient = LocalPlayer->ViewportClient)
        {
            if (const FViewport* Viewport = ViewportClient->Viewport)
            {
                const FIntPoint Size = Viewport->GetSizeXY();
                Result.ViewportSize = FVector2D(
                    static_cast<double>(Size.X),
                    static_cast<double>(Size.Y));

                if (const UUserInterfaceSettings* UISettings =
                    GetDefault<UUserInterfaceSettings>())
                {
                    Result.DPIScale = UISettings->GetDPIScaleBasedOnSize(Size);
                }
            }
        }
    }

    if (Result.ViewportSize.Y > KINDA_SMALL_NUMBER)
    {
        Result.AspectRatio =
            Result.ViewportSize.X / Result.ViewportSize.Y;
    }

    Result.Orientation =
        Result.ViewportSize.Y > Result.ViewportSize.X
            ? EGamePlatformUIOrientation::Portrait
            : EGamePlatformUIOrientation::Landscape;

    Result.LayoutClass = ResolveLayoutClass(
        Result.ViewportSize,
        Result.DPIScale);

    if (FSlateApplication::IsInitialized() &&
        Result.ViewportSize.X > 0.0 &&
        Result.ViewportSize.Y > 0.0)
    {
        // 使用 Slate 官方安全区计算结果；不在业务 Widget 中重复查询。
        FSlateApplication::Get().GetSafeZoneSize(
            Result.SafeZonePadding,
            Result.ViewportSize);
    }

    Result.bTouchCapable =
        Result.PlatformClass == EGamePlatformUIPlatformClass::Mobile ||
        Result.InputDeviceClass == EGamePlatformUIInputDeviceClass::Touch;

    return Result;
}
