#pragma once

#include "Interfaces/IGamePlatformSettingsService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformSettingsSubsystem.generated.h"

class FGamePlatformSettingsRegistry;
class IGamePlatformSettingsPersistenceProvider;
class IModularFeature;

/**
 * 游戏平台设置运行时统一管理子系统。
 *
 * 生命周期属于 GameInstance：跨登录、角色选择及不同地图／体验切换保持，
 * 但不同 PIE/GameInstance 之间完全隔离。整个 Runtime 不实现 Tick。
 */
UCLASS()
class GAMEPLATFORMSETTINGSRUNTIME_API UGamePlatformSettingsSubsystem final
    : public UGameInstanceSubsystem
    , public IGamePlatformSettingsService
{
    GENERATED_BODY()

public:
    UGamePlatformSettingsSubsystem();
    UGamePlatformSettingsSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformSettingsSubsystem() override;

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual bool GetValue(
        FName SettingId,
        FGamePlatformSettingValue& OutValue) const override;
    virtual bool GetDescriptor(
        FName SettingId,
        FGamePlatformSettingDescriptor& OutDescriptor) const override;
    virtual FGamePlatformResult SetValue(
        FName SettingId,
        EGamePlatformSettingLayer Layer,
        const FGamePlatformSettingValue& Value) override;
    virtual FGamePlatformResult ResetValue(
        FName SettingId,
        EGamePlatformSettingLayer Layer) override;
    virtual FGamePlatformResult ResetCategory(
        FName Category,
        EGamePlatformSettingLayer Layer) override;
    virtual FGamePlatformResult Apply(
        EGamePlatformSettingsChangeReason Reason) override;
    virtual FGamePlatformResult Save() override;
    virtual FGamePlatformResult Reload() override;
    virtual FGamePlatformResult SwitchUserContext(
        const FString& UserContextKey) override;
    virtual FGamePlatformSettingsSnapshot GetSnapshot() const override;
    virtual FGamePlatformSettingsRuntimeDiagnostics GetDiagnostics() const override;
    virtual FGamePlatformSettingsRuntimeSubscription Subscribe(
        TWeakObjectPtr<UObject> Owner,
        FGamePlatformSettingsRuntimeChangedCallback Callback,
        FGamePlatformResult& OutResult) override;
    virtual bool Unsubscribe(
        const FGamePlatformSettingsRuntimeSubscription& Subscription) override;

private:
    struct FSettingsRuntimeSubscriptionEntry
    {
        TWeakObjectPtr<UObject> Owner;
        FGamePlatformSettingsRuntimeChangedCallback Callback;
    };

    EGamePlatformSettingRuntimeScope GetCurrentRuntimeScope() const;
    bool CanMutate(FGamePlatformResult& OutResult);
    bool IsOwnerInScope(const TWeakObjectPtr<UObject>& Owner) const;
    bool IsPublicMutableLayer(EGamePlatformSettingLayer Layer) const;

    FGamePlatformResult ReloadInternal(
        EGamePlatformSettingsChangeReason Reason);
    void BuildDefaultLayers(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        TMap<EGamePlatformSettingLayer,
            TMap<FName, FGamePlatformSettingValue>>& OutLayers) const;
    FGamePlatformResult SanitizePersistencePayload(
        const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
        struct FGamePlatformSettingsPersistencePayload& InOutPayload) const;
    FGamePlatformResult ApplyMigrations(
        struct FGamePlatformSettingsPersistencePayload& InOutPayload,
        int32 TargetVersion);
    FGamePlatformResult ResolvePersistenceProvider(
        IGamePlatformSettingsPersistenceProvider*& OutProvider) const;
    FGamePlatformResult ResolveAndPublish(
        EGamePlatformSettingsChangeReason Reason,
        bool bForceNotification);
    void PublishChanges(const FGamePlatformSettingsChangeSet& ChangeSet);
    void HandleSaveCompleted(
        uint64 SavedMutationGeneration,
        const FGamePlatformResult& Result);

    void HandleModularFeatureRegistered(
        const FName& Type,
        IModularFeature* Feature);
    void HandleModularFeatureUnregistered(
        const FName& Type,
        IModularFeature* Feature);
    void HandleFeatureTopologyChanged(const FName& Type);

    FGuid ScopeId;
    uint64 Generation = 0;
    uint64 MutationGeneration = 0;
    /** 当前不透明用户上下文键；只用于协调持久化Provider，不进入日志/Snapshot。 */
    FString CurrentUserContextKey;

    TUniquePtr<FGamePlatformSettingsRegistry> Registry;
    TMap<EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>> Layers;

    FGamePlatformSettingsSnapshot Snapshot;
    FGamePlatformSettingsRuntimeDiagnostics Diagnostics;
    TMap<FGuid, FSettingsRuntimeSubscriptionEntry> Subscriptions;

    FDelegateHandle FeatureRegisteredHandle;
    FDelegateHandle FeatureUnregisteredHandle;

    bool bPublishing = false;
    bool bSaveInFlight = false;
    /** 异步保存期间发生的Provider拓扑变化延后处理，防止替换正在保存代次对应的Registry/Layers。 */
    bool bPendingTopologyReload = false;
    bool bUserDirty = false;
    bool bPendingResolve = false;
    bool bDeinitializing = false;
};
