#pragma once

#include "Interfaces/IGamePlatformSaveService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformSaveSubsystem.generated.h"
class FGamePlatformSaveProcessSlotLease; // 私有线程安全租约，只在.cpp构造；无需反射类型参与UHT。

/**
 * GamePlatformSave（游戏平台本地存档）GameInstance级执行器。
 * 公开调用和Provider迁移仅在Game Thread执行；实际文件IO在线程池执行。
 * 同进程各GameInstance共用物理Key互斥租约，取消/销毁旧实例不提前释放后台IO占有。
 */
UCLASS(Transient)
class UGamePlatformSaveSubsystem final
    : public UGameInstanceSubsystem
    , public IGamePlatformSaveService
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual FGamePlatformResult RegisterProvider(
        const TSharedRef<IGamePlatformSaveProvider>& Provider) override;
    virtual FGamePlatformResult UnregisterProvider(FName Namespace) override;

    virtual FGamePlatformSaveRequestHandle SaveRecordAsync(
        const FGamePlatformSaveRecord& Record,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) override;

    virtual FGamePlatformSaveRequestHandle LoadRecordAsync(
        const FGamePlatformSaveKey& Key,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) override;

    virtual FGamePlatformSaveRequestHandle DeleteRecordAsync(
        const FGamePlatformSaveKey& Key,
        FGamePlatformSaveCallback Callback,
        FGamePlatformResult& OutStartResult) override;

    virtual FGamePlatformSaveDiagnostics GetDiagnostics() const override;

private:
    struct FPendingRequest
    {
        FGamePlatformSaveRequestHandle Handle;
        FGamePlatformSaveKey Key;
        FString StableStorageId;
        /** 本GI回调与后台任务共享的进程槽位占有；取消只能撤销回调，不能提前解锁磁盘IO。 */
        TSharedPtr<FGamePlatformSaveProcessSlotLease, ESPMode::ThreadSafe> ProcessSlotLease;
        EGamePlatformSaveOperation Operation = EGamePlatformSaveOperation::Load;
        FGamePlatformSaveCallback Callback;
        TSharedPtr<IGamePlatformSaveProvider> ProviderSnapshot;
    };

    bool IsMutationEnvironmentAllowed() const;
    TSharedPtr<IGamePlatformSaveProvider> FindProvider(FName Namespace) const;

    FGamePlatformSaveRequestHandle BeginRequest(
        const FGamePlatformSaveKey& Key,
        EGamePlatformSaveOperation Operation,
        FGamePlatformSaveCallback Callback,
        TSharedPtr<IGamePlatformSaveProvider> ProviderSnapshot,
        FGamePlatformResult& OutStartResult);

    void CompleteRequest(
        const FGamePlatformSaveRequestHandle& Handle,
        FGamePlatformSaveOperationResult OperationResult);

    void CancelPendingRequests();

    FGuid ScopeId;
    uint64 Generation = 0;
    TMap<FName, TSharedPtr<IGamePlatformSaveProvider>> Providers;
    TMap<FGuid, FPendingRequest> PendingRequests;
    TSet<FString> BusyStorageIds;
    FGamePlatformSaveDiagnostics Diagnostics;
    bool bDeliveringCallback = false;
};
