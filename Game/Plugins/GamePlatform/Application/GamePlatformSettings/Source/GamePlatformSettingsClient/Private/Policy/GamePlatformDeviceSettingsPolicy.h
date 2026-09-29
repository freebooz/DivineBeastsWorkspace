#pragma once

#include "Types/GamePlatformDeviceSettingsTypes.h"

/** 纯值校验策略；不访问磁盘、视口、设备或 UObject，便于自动化测试。 */
struct FGamePlatformDeviceSettingsPolicy final
{
    /** 严格校验候选值；不静默钳制调用方输入。 */
    static FGamePlatformResult Validate(
        const FGamePlatformDeviceSettings& Settings);

    /** 语义等价比较；浮点字段使用小容差，避免无意义状态广播。 */
    static bool AreEquivalent(
        const FGamePlatformDeviceSettings& A,
        const FGamePlatformDeviceSettings& B);
};
