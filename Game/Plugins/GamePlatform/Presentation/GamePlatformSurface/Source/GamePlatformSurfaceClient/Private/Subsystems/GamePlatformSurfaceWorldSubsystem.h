// 本文件属于GamePlatform平台层 GamePlatformSurface，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "TimerManager.h"
#include "GamePlatformSurfaceWorldSubsystem.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

/**
 * GamePlatformSurface世界服务。
 *
 * 只在客户端可表现世界中创建；无Tick。状态变化时一次性写入MPC，并在世界销毁时释放绑定和订阅。
 */
UCLASS()
class UGamePlatformSurfaceWorldSubsystem final : public UWorldSubsystem, public IGamePlatformSurfaceService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformSurfaceUpdateResult ApplyEnvironmentState(
        const FGamePlatformSurfaceEnvironmentState& State) override;
    virtual const FGamePlatformSurfaceEnvironmentState& GetEnvironmentState() const override;
    virtual int32 GetRevision() const override;
    virtual FGamePlatformSurfaceUpdateResult RefreshMaterialBinding() override;
    virtual FDelegateHandle AddStateChangedHandler(
        const FGamePlatformSurfaceStateChanged::FDelegate& Handler) override;
    virtual void RemoveStateChangedHandler(FDelegateHandle Handle) override;
    virtual bool SubmitSnowContact(const FGamePlatformSnowContact& Contact) override;
    virtual bool RegisterSnowContactMaterial(UMaterialInstanceDynamic& Material) override;

private:
    friend class FGamePlatformSurfaceBindingRefreshTest;
    friend class FGamePlatformSurfaceWindBindingTest;
    /** 首次申请Data普通资源租约；等待/失败不会因状态事件重复申请，只有Refresh换代重试。 */
    bool ResolveMaterialBinding();
    void HandleMaterialBindingLoaded(int64 RequestGeneration, const FGamePlatformDataLease& Lease,
        const FGamePlatformResult& Result);
    /** 先换代再释放自有Data租约，旧回调不得覆盖新配置或世界。 */
    void ReleaseMaterialBinding();

    /** 把CurrentState一次性写入已绑定MPC；调用前必须位于游戏线程。 */
    FGamePlatformSurfaceUpdateResult PushCurrentStateToMaterialParameters();

    FGamePlatformSurfaceEnvironmentState CurrentState;
    int32 Revision = 0;
    bool bClosing = false;
    bool bLoggedBindingFailure = false;
    bool bBindingAttempted = false;
    int64 BindingGeneration = 0;
    FGamePlatformDataLease BindingLease;
    FGamePlatformSurfaceUpdateResult LastBindingResult;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialParameterCollection> BoundCollection;

    TWeakObjectPtr<UMaterialParameterCollectionInstance> BoundInstance;
    FGamePlatformSurfaceStateChanged StateChanged;

    // 局部压雪归同一Surface世界服务；实现分离到Private/Snow，不新增Subsystem/资产管理器。
    bool BeginSnowContactLoad();
    void OnSnowContactMaterialLoaded(int64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void FlushSnowContacts();
    void BindSnowContactReceivers();
    void ClearSnowContacts();
    void ReleaseSnowContacts();
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> SnowContactField;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> SnowContactScratch;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SnowContactUpdateMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SnowContactCopyMID;
    TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> SnowContactReceivers;
    TArray<FGamePlatformSnowContact> PendingSnowContacts;
    TArray<FGuid> SnowContactRecentIds;
    FGamePlatformDataLease SnowContactLease;
    FTimerHandle SnowContactDecayTimer;
    FTimerHandle SnowContactFlushTimer;
    int64 SnowContactGeneration = 0;
    FVector2D SnowContactOrigin = FVector2D::ZeroVector;
    double SnowContactLastUpdate = 0.;
    double SnowContactLastStamp = -1.;
    int32 SnowContactSubmitted = 0;
    bool bSnowContactLoadAttempted = false;
    bool bSnowContactFlushQueued = false;
};
