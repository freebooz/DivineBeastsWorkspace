#pragma once

#include "Interfaces/IGamePlatformSettingsProvider.h"

/**
 * Dedicated Server 配置持久化/部署 Provider。
 * 只读取 INI、Environment、CommandLine 并形成只读分层值，不依赖客户端模块或表现资源。
 */
class FGamePlatformSettingsServerPersistenceProvider final
    : public IGamePlatformSettingsPersistenceProvider
{
public:
    virtual FName GetPersistenceId() const override;
    virtual bool SupportsRuntime(
        EGamePlatformSettingRuntimeScope RuntimeScope) const override;
    virtual FGamePlatformResult SetUserContext(
        const FString& UserContextKey) override;
    virtual FGamePlatformResult Load(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        FGamePlatformSettingsPersistencePayload& OutPayload) override;
    virtual FGamePlatformResult BeginSave(
        const TMap<FName, FGamePlatformSettingValue>& UserValues,
        int32 SchemaVersion,
        FGamePlatformSettingsSaveCompletion Completion) override;

private:
    static FString MakeEnvironmentKey(FName SettingId);
    static FGamePlatformResult ParseAndAdd(
        const FGamePlatformSettingDescriptor& Descriptor,
        const FString& RawValue,
        EGamePlatformSettingLayer Layer,
        FGamePlatformSettingsPersistencePayload& OutPayload);
};
