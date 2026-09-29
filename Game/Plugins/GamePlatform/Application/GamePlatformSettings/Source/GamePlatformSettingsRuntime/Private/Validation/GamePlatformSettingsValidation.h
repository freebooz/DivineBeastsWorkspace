#pragma once

#include "Types/GamePlatformSettingTypes.h"

/** Runtime 内部统一校验；Registry、持久层与 SetValue 共用同一规则，避免语义漂移。 */
struct FGamePlatformSettingsValidation final
{
    static FGamePlatformResult ValidateDescriptor(
        const FGamePlatformSettingDescriptor& Descriptor);

    static FGamePlatformResult ValidateValue(
        const FGamePlatformSettingDescriptor& Descriptor,
        const FGamePlatformSettingValue& Value);

    static bool SupportsRuntime(
        const FGamePlatformSettingDescriptor& Descriptor,
        EGamePlatformSettingRuntimeScope CurrentRuntime);

    static bool IsLayerAllowed(
        const FGamePlatformSettingDescriptor& Descriptor,
        EGamePlatformSettingLayer Layer,
        EGamePlatformSettingRuntimeScope CurrentRuntime);
};
