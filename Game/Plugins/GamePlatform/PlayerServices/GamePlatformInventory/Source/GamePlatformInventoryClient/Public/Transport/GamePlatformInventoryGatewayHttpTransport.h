#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformInventoryClientTransport.h"

/**
 * Gateway（网关）Inventory异步HTTP传输。
 * AccessToken（访问令牌）必须由已完成认证的Online/Identity（在线/身份）层注入；
 * 本类不接收PlayerId，也不接触PlayerData内部Token。
 */
class GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryGatewayHttpTransport final
    : public IGamePlatformInventoryClientTransport
    , public TSharedFromThis<
        FGamePlatformInventoryGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformInventoryGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    bool IsConfigured() const;

    virtual void CancelAllRequests() override;

    virtual bool BeginGetSnapshot(
        FGamePlatformInventorySnapshotCompletion Completion) override;

    virtual bool BeginGetOperation(
        const FGuid& OperationId,
        FGamePlatformInventoryMutationCompletion Completion) override;

    virtual bool BeginMove(
        const FGamePlatformInventoryMoveRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) override;

    virtual bool BeginSplit(
        const FGamePlatformInventorySplitRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) override;

    virtual bool BeginMerge(
        const FGamePlatformInventoryMergeRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) override;

    virtual bool BeginSetQuickbar(
        const FGamePlatformInventoryQuickbarRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) override;

    virtual bool BeginClearQuickbar(
        const FGamePlatformInventoryQuickbarRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) override;

private:
    FString GatewayBaseUrl;
    FString AccessToken;

    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<class FJsonObject>& Body,
        TFunction<void(int32, const FString&)> Completion);

    bool BeginMutation(
        const FString& Path,
        const TSharedPtr<class FJsonObject>& Body,
        FGamePlatformInventoryMutationCompletion Completion);

    FCriticalSection ActiveRequestsMutex;
    TArray<TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;

    void UnregisterRequest(
        const TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>& Request);

    static FString GuidString(const FGuid& Guid);

    static bool JsonToSnapshot(
        const TSharedPtr<class FJsonObject>& Json,
        FGamePlatformInventorySnapshot& OutSnapshot);

    static bool JsonToMutationResult(
        const TSharedPtr<class FJsonObject>& Json,
        FGamePlatformInventoryMutationResult& OutResult);

    static EGamePlatformInventoryError MapHttpError(
        int32 StatusCode,
        const FString& Body);
};
