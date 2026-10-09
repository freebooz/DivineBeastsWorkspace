#pragma once

#include "Interfaces/IGamePlatformSettingsProvider.h"

/**
 * 客户端用户档案持久化 Provider。
 * BeginLoad使用UE原生异步存在性查询后调用AsyncLoadGameFromSlot；Load只提供无用户/PIE空内存层，禁止同步磁盘读取。
 * BeginSave使用UE AsyncSaveGameToSlot；回调只捕获值，不依赖Provider寿命。
 */
class FGamePlatformSettingsClientPersistenceProvider final
    : public IGamePlatformSettingsPersistenceProvider
{
public:
    /** 无用户状态的注册工厂为每个GI创建独立实例，隔离交错Load/Save。 */
    virtual TUniquePtr<IGamePlatformSettingsPersistenceProvider> CreateScopedProvider() const override;
    virtual FName GetPersistenceId() const override;
    virtual bool SupportsRuntime(
        EGamePlatformSettingRuntimeScope RuntimeScope) const override;
    virtual FGamePlatformResult SetUserContext(
        const FString& UserContextKey) override;
    virtual FGamePlatformResult Load(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        FGamePlatformSettingsPersistencePayload& OutPayload) override;
    virtual FGamePlatformResult BeginLoad(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        FGamePlatformSettingsLoadCompletion Completion) override;
    virtual FGamePlatformResult BeginSave(
        const TMap<FName, FGamePlatformSettingValue>& UserValues,
        int32 SchemaVersion,
        FGamePlatformSettingsSaveCompletion Completion) override;

private:
    /** 根据基础槽名和不透明用户键生成不泄露原始账号标识的本地槽名。 */
    FString GetScopedProfileSlotName() const;

    /** 仅GI独占克隆持有账号键；模块注册对象不调用SetUserContext。 */
    FString CurrentUserContextKey;
};
