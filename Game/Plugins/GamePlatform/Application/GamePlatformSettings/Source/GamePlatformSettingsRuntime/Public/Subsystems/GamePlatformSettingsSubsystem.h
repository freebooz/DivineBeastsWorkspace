// 平台GI设置运行服务：中立描述/解析/用户层；每GI拥有持久化克隆和用户键，设备层由Client单独消费；退出失效保存回调。
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
    bool CanMutate(FGamePlatformResult& OutResult, bool bAllowLoadInFlight = false);
    bool IsOwnerInScope(const TWeakObjectPtr<UObject>& Owner) const;
    bool IsPublicMutableLayer(EGamePlatformSettingLayer Layer) const;

    FGamePlatformResult ReloadInternal(
        EGamePlatformSettingsChangeReason Reason);
    /** 仅游戏线程原子发布当前读取代次；重复/过期/销毁回调不访问PendingRegistry或用户层。 */
    void HandleLoadCompleted(uint64 ExpectedLoadGeneration, struct FGamePlatformSettingsPersistencePayload Payload, const FGamePlatformResult& Result);
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
        uint64 ExpectedInstanceGeneration,
        uint64 ExpectedSaveRequestGeneration,
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

    /** 客户端持久化由本GI独占；缓存工厂身份只用于拓扑变化时重新创建，不访问其他GI账号键。 */
    mutable TSharedPtr<IGamePlatformSettingsPersistenceProvider> ScopedPersistenceProvider;
    mutable IGamePlatformSettingsPersistenceProvider* PersistenceFactory = nullptr;
    /** 在飞候选注册表不对外可见，成功完成全部校验与迁移后才替换已发布注册表。 */
    TUniquePtr<FGamePlatformSettingsRegistry> PendingLoadRegistry;
    uint64 LoadGeneration = 0;
    int32 PendingLoadTargetVersion = 1;
    EGamePlatformSettingsChangeReason PendingLoadReason = EGamePlatformSettingsChangeReason::Reload;
    bool bLoadInFlight = false;
    TUniquePtr<FGamePlatformSettingsRegistry> Registry;
    TMap<EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>> Layers;

    FGamePlatformSettingsSnapshot Snapshot;
    FGamePlatformSettingsRuntimeDiagnostics Diagnostics;
    TMap<FGuid, FSettingsRuntimeSubscriptionEntry> Subscriptions;

    FDelegateHandle FeatureRegisteredHandle;
    FDelegateHandle FeatureUnregisteredHandle;

    bool bPublishing = false;
    /** 每次保存独立终态身份；与用户变更代次分离，重复旧Completion不能消费新请求。 */
    uint64 SaveRequestGeneration = 0;
    bool bSaveInFlight = false;
    /** 异步保存期间发生的Provider拓扑变化延后处理，防止替换正在保存代次对应的Registry/Layers。 */
    bool bPendingTopologyReload = false;
    /** 广播中拓扑事件合并为至多一个GT唤醒，防止同步重建混合正在发布的候选。 */
    bool bTopologyWakeQueued = false;
    bool bUserDirty = false;
    bool bPendingResolve = false;
    bool bDeinitializing = false;
};
