#pragma once

#include "Interfaces/IGamePlatformSettingsProvider.h"

/**
 * 设置注册表。
 * Rebuild 使用临时容器事务构建；任何 Provider/SettingId 冲突都会保留旧注册表不变。
 */
class FGamePlatformSettingsRegistry final
{
public:
    FGamePlatformResult Rebuild(
        int32 MaxProviders,
        int32 MaxDescriptors,
        EGamePlatformSettingRuntimeScope CurrentRuntime);

    const TMap<FName, FGamePlatformSettingDescriptor>& GetDescriptors() const
    {
        return Descriptors;
    }

    const TMap<FName, FName>& GetOwners() const
    {
        return OwnerBySettingId;
    }

    int32 GetProviderCount() const
    {
        return ProviderCount;
    }

private:
    TMap<FName, FGamePlatformSettingDescriptor> Descriptors;
    TMap<FName, FName> OwnerBySettingId;
    int32 ProviderCount = 0;
};
