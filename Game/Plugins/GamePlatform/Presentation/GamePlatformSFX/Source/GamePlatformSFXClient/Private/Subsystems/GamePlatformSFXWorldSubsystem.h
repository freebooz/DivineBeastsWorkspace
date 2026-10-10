// 本文件属于GamePlatform平台层 GamePlatformSFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "Interfaces/IGamePlatformSFXService.h"
#include "Components/AudioComponent.h"
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
    virtual FGamePlatformSFXPlaybackSnapshot GetPlaybackSnapshot(const FGamePlatformSFXHandle& Handle) const override;
    virtual FDelegateHandle AddCompletionHandler(const FGamePlatformSFXPlaybackCompleted::FDelegate& Handler) override;
    virtual void RemoveCompletionHandler(FDelegateHandle Handle) override;
    virtual bool SetFloatParameter(const FGamePlatformSFXHandle& Handle, FName Name, float Value) override;
    virtual bool SetVolumeMultiplier(const FGamePlatformSFXHandle& Handle, float Value) override;
    virtual FGamePlatformSFXDiagnostics GetDiagnostics() const override;

private:
    friend class FGamePlatformSFXLifecycleRegressionTest;
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
    /** UE启动拒绝亦广播Stopped；自然完成/失败/Stop共享幂等回收。 */
    void HandleAudioPlayStateChanged(const UAudioComponent* Component, EAudioComponentPlayState PlayState);
    void FailPending(FGuid HandleId, EGamePlatformSFXResultCode Code);
    void CleanupActive(FGuid HandleId, bool bStopComponent,
        EGamePlatformSFXPlaybackState State = EGamePlatformSFXPlaybackState::Completed,
        EGamePlatformSFXResultCode Code = EGamePlatformSFXResultCode::AlreadyCompleted);
    void CompletePlayback(const FGamePlatformSFXHandle& Handle, EGamePlatformSFXPlaybackState State, EGamePlatformSFXResultCode Code);
    TMap<FGuid, FGamePlatformSFXPlaybackSnapshot> PlaybackSnapshots;
    TArray<FGuid> CompletedPlaybackOrder;
    FGamePlatformSFXPlaybackCompleted PlaybackCompleted;
    void ReleaseLease(const FGamePlatformDataLease& Lease) const;
    void RemoveRequestMapping(const FGuid& RequestId, const FGuid& HandleId);

    struct FTerminalOccurrence { double ExpiresAtSeconds = 0.0; bool bCancelled = false; EGamePlatformSFXResultCode CompletionCode = EGamePlatformSFXResultCode::AlreadyCompleted; };
    void PruneTerminalOccurrences();
    void RecordTerminalOccurrence(const FGuid& RequestId, bool bCancelled);
    /** World局部终态最多512项、30秒；不持有UObject或Data Lease。 */
    TMap<FGuid, FTerminalOccurrence> TerminalOccurrences;

    int32 Generation = 1;
    bool bClosing = false;
    TMap<FGuid, FPendingPlay> PendingPlays;
    TMap<FGuid, FActiveInstance> ActiveInstances;
    TMap<FGuid, FGamePlatformSFXHandle> RequestHandles;
    FGamePlatformSFXDiagnostics Diagnostics;
};
