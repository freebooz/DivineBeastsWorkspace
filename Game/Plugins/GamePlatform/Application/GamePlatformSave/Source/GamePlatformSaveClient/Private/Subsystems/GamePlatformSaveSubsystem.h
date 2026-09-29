#pragma once

#include "Interfaces/IGamePlatformSaveService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformSaveSubsystem.generated.h"

/**
 * GamePlatformSave（游戏平台本地存档）GameInstance级执行器。
 *
 * 公开调用和Provider迁移均只在Game Thread（游戏线程）执行；实际文件IO在线程池执行。
 * 同一逻辑Key同时只允许一个Load/Save/Delete，避免本地文件竞态。
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
