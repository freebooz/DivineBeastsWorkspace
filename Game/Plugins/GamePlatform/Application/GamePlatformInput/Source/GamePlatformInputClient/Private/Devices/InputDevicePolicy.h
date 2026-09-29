#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformInputProfileDefinition.h"

/**
 * GamePlatformInputDevicePolicy（游戏平台输入设备策略）。
 * 只负责“目标平台默认设备”和“Profile禁用设备后的回退选择”，不监听硬件、不创建UI、不重建Mapping。
 * 该策略位于Private，避免把平台编译宏和设备回退细节暴露为公共API。
 */
namespace GamePlatformInputDevicePolicy
{
/** Android/iOS默认Touch；桌面默认KeyboardMouse，避免触屏PC仅因支持触摸就误用移动提示。 */
inline constexpr EGamePlatformInputDeviceFamily GetDefaultDeviceFamilyForTarget()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    return EGamePlatformInputDeviceFamily::Touch;
#else
    return EGamePlatformInputDeviceFamily::KeyboardMouse;
#endif
}

/** Profile禁用当前设备时选择稳定回退；移动目标优先Touch，桌面目标优先KeyboardMouse。 */
inline EGamePlatformInputDeviceFamily SelectFallbackDeviceFamily(
    const UGamePlatformInputProfileDefinition& Profile)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    if (Profile.bEnableTouch) { return EGamePlatformInputDeviceFamily::Touch; }
    if (Profile.bEnableGamepad) { return EGamePlatformInputDeviceFamily::Gamepad; }
    if (Profile.bEnableKeyboardMouse) { return EGamePlatformInputDeviceFamily::KeyboardMouse; }
#else
    if (Profile.bEnableKeyboardMouse) { return EGamePlatformInputDeviceFamily::KeyboardMouse; }
    if (Profile.bEnableGamepad) { return EGamePlatformInputDeviceFamily::Gamepad; }
    if (Profile.bEnableTouch) { return EGamePlatformInputDeviceFamily::Touch; }
#endif
    return EGamePlatformInputDeviceFamily::Unknown;
}
}
