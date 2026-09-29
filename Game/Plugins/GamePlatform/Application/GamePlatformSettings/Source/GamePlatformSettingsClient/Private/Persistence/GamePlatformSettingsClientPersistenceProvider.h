#pragma once

#include "Interfaces/IGamePlatformSettingsProvider.h"

/**
 * 客户端用户档案持久化 Provider。
 * Load 在启动/显式Reload时同步读取一次；Save 使用 UE AsyncSaveGameToSlot，不进入Slider高频路径。
 */
class FGamePlatformSettingsClientPersistenceProvider final
    : public IGamePlatformSettingsPersistenceProvider
{
public:
    virtual FName GetPersistenceId() const override;
    virtual bool SupportsRuntime(
        EGamePlatformSettingRuntimeScope RuntimeScope) const override;
    virtual FGamePlatformResult Load(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        FGamePlatformSettingsPersistencePayload& OutPayload) override;
    virtual FGamePlatformResult BeginSave(
        const TMap<FName, FGamePlatformSettingValue>& UserValues,
        int32 SchemaVersion,
        FGamePlatformSettingsSaveCompletion Completion) override;
};
