#pragma once

#include "Types/GamePlatformSettingTypes.h"

using FGamePlatformSettingsLayers =
    TMap<EGamePlatformSettingLayer, TMap<FName, FGamePlatformSettingValue>>;

/** 纯值分层解析器；无 UObject、磁盘或模块查询，可独立自动化测试。 */
struct FGamePlatformSettingsResolver final
{
    static FGamePlatformResult Resolve(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        const FGamePlatformSettingsLayers& Layers,
        EGamePlatformSettingRuntimeScope CurrentRuntime,
        const FGamePlatformSettingsSnapshot& Previous,
        EGamePlatformSettingsChangeReason Reason,
        int32 SchemaVersion,
        bool bDirty,
        FGamePlatformSettingsSnapshot& OutSnapshot,
        FGamePlatformSettingsChangeSet& OutChanges);
};
