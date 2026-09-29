#pragma once

#include "Interfaces/IGamePlatformDeviceSettingsService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformDeviceSettingsSubsystem.generated.h"

/** 私有 GameInstance 级设置执行器，不公开 UGameUserSettings 可变指针。 */
UCLASS(Transient)
class UGamePlatformDeviceSettingsSubsystem final
    : public UGameInstanceSubsystem
    , public IGamePlatformDeviceSettingsService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformDeviceSettingsSnapshot GetSnapshot() const override;
    virtual FGamePlatformDeviceSettingsDiagnostics GetDiagnostics() const override;
    virtual FGamePlatformResult ReloadFromDisk() override;
    virtual FGamePlatformResult StageDeviceSettings(
        const FGamePlatformDeviceSettings& Settings) override;
    virtual FGamePlatformResult ApplyStagedSettings(
        EGamePlatformDeviceSettingsApplyMode ApplyMode) override;
    virtual FGamePlatformResult ConfirmPreview() override;
    virtual FGamePlatformResult CancelPreview() override;
    virtual FGamePlatformResult DiscardStagedSettings() override;
    virtual FGamePlatformDeviceSettingsSubscription Subscribe(
        TWeakObjectPtr<UObject> Owner,
        FGamePlatformDeviceSettingsChangedCallback Callback,
        FGamePlatformResult& OutResult) override;
    virtual bool Unsubscribe(
        const FGamePlatformDeviceSettingsSubscription& Subscription) override;

private:
    struct FDeviceSettingsSubscriptionEntry
    {
        TWeakObjectPtr<UObject> Owner;
        FGamePlatformDeviceSettingsChangedCallback Callback;
    };

    bool IsMutationEnvironmentAllowed() const;
    bool CanMutate(FGamePlatformResult& OutResult);
    bool IsOwnerInScope(const TWeakObjectPtr<UObject>& Owner) const;

    FGamePlatformResult ReadCurrentSettings(
        FGamePlatformDeviceSettings& OutSettings) const;
    FGamePlatformResult ApplyToEngine(
        const FGamePlatformDeviceSettings& Settings) const;

    void RefreshDerivedState();
    void PublishSnapshot();

    FGuid ScopeId;
    uint64 Generation = 0;
    FGamePlatformDeviceSettingsSnapshot Snapshot;
    FGamePlatformDeviceSettingsDiagnostics Diagnostics;
    FGamePlatformDeviceSettings PreviewBaseline;
    TMap<FGuid, FDeviceSettingsSubscriptionEntry> Subscriptions;
    bool bPublishing = false;
};
