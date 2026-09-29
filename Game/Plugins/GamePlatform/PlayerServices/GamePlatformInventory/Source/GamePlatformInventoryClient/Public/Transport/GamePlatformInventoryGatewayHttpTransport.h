#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformInventoryClientTransport.h"

class UGamePlatformOnlineClientSubsystem;

/**
 * FGamePlatformInventoryGatewayHttpTransport（背包网关传输）。
 *
 * 本类不拥有 AccessToken（访问令牌），所有受保护请求统一委托给
 * GamePlatformOnlineClient（平台在线客户端）发送，从而复用认证刷新、超时、
 * 同源校验、请求取消和安全重放策略。客户端永远不传 PlayerId（玩家编号）。
 */
class GAMEPLATFORMINVENTORYCLIENT_API FGamePlatformInventoryGatewayHttpTransport final
    : public IGamePlatformInventoryClientTransport
    , public TSharedFromThis<
        FGamePlatformInventoryGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    explicit FGamePlatformInventoryGatewayHttpTransport(
        UGamePlatformOnlineClientSubsystem* InOnlineSubsystem);
    virtual ~FGamePlatformInventoryGatewayHttpTransport() override;

    /** 当前 Online（在线）子系统是否仍有有效认证上下文。 */
    bool IsConfigured() const;

    /** 只取消本 Transport 发起的背包请求，不取消同一 GameInstance 的其他在线请求。 */
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
    struct FRuntime;
    TUniquePtr<FRuntime> Runtime;

    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<class FJsonObject>& Body,
        const FString& IdempotencyKey,
        TFunction<void(
            int32,
            const FString&,
            EGamePlatformInventoryError)> Completion);

    bool BeginMutation(
        const FString& Path,
        const FGuid& OperationId,
        const TSharedPtr<class FJsonObject>& Body,
        FGamePlatformInventoryMutationCompletion Completion);

    void UnregisterRequest(const FGuid& RequestId);

    static FString GuidString(const FGuid& Guid);
    static FString RevisionString(int64 Revision);

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

