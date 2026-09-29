#pragma once

#include "Interfaces/IGamePlatformSFXService.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "GamePlatformSFXWorldSubsystem.generated.h"

class UAudioComponent;
class UGamePlatformSFXDefinition;

/**
 * UGamePlatformSFXWorldSubsystem（游戏平台音效世界执行器）。
 * 只在客户端游戏世界创建；无Tick，通过Data回调与AudioFinished事件驱动生命周期。
 */
UCLASS()
class UGamePlatformSFXWorldSubsystem final : public UWorldSubsystem, public IGamePlatformSFXService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformSFXResult Play(const FGamePlatformSFXRequest& Request) override;
    virtual bool Stop(const FGamePlatformSFXHandle& Handle, float FadeOutSeconds = -1.0f) override;
    virtual bool StopByRequestId(const FGuid& RequestId, float FadeOutSeconds = -1.0f) override;
    virtual bool IsActive(const FGamePlatformSFXHandle& Handle) const override;
    virtual bool SetFloatParameter(const FGamePlatformSFXHandle& Handle, FName Name, float Value) override;
    virtual bool SetVolumeMultiplier(const FGamePlatformSFXHandle& Handle, float Value) override;
    virtual FGamePlatformSFXDiagnostics GetDiagnostics() const override;

private:
    struct FPendingPlay
    {
        FGamePlatformSFXHandle Handle;
        FGamePlatformSFXRequest Request;
        FGamePlatformDataLease Lease;
    };

    struct FActiveInstance
    {
        FGamePlatformSFXHandle Handle;
        FGuid RequestId;
        TWeakObjectPtr<UAudioComponent> Component;
        FGamePlatformDataLease Lease;
        TSet<FName> AllowedFloatParameters;
        float DefaultFadeOutSeconds = 0.0f;
        bool bStopRequested = false;
    };

    bool IsHandleCurrent(const FGamePlatformSFXHandle& Handle) const;
    void HandleDefinitionLoaded(
        FGamePlatformSFXHandle Handle,
        const FGamePlatformDataLease& Lease,
        const FGamePlatformResult& Result);
    void HandleAudioFinished(UAudioComponent* Component);
    void FailPending(FGuid HandleId, EGamePlatformSFXResultCode Code);
    void CleanupActive(FGuid HandleId, bool bStopComponent);
    void ReleaseLease(const FGamePlatformDataLease& Lease) const;
    void RemoveRequestMapping(const FGuid& RequestId, const FGuid& HandleId);

    int32 Generation = 1;
    bool bClosing = false;
    TMap<FGuid, FPendingPlay> PendingPlays;
    TMap<FGuid, FActiveInstance> ActiveInstances;
    TMap<FGuid, FGamePlatformSFXHandle> RequestHandles;
    FGamePlatformSFXDiagnostics Diagnostics;
};
