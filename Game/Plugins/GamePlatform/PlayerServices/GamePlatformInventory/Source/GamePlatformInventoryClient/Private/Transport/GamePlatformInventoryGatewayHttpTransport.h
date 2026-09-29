#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformInventoryClientTransport.h"

class UGamePlatformOnlineClientSubsystem;

/**
 * FGamePlatformInventoryGatewayHttpTransport（背包网关在线适配器）。
 *
 * 这是 GamePlatformInventoryClient（背包客户端模块）的私有默认实现：
 * - 复用 GamePlatformOnlineClient（平台在线客户端）的认证请求通道；
 * - 不保存 AccessToken（访问令牌），不接受 PlayerId（玩家编号）；
 * - 统一继承 Online 的刷新、超时、取消、同源与幂等重放策略。
 *
 * 对外稳定边界只有 IGamePlatformInventoryClientTransport（背包客户端传输接口），
 * 业务模块不得直接依赖本类，避免把具体 Online 实现扩散为公共 API。
 */
class FGamePlatformInventoryGatewayHttpTransport final
    : public IGamePlatformInventoryClientTransport
    , public TSharedFromThis<
        FGamePlatformInventoryGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    explicit FGamePlatformInventoryGatewayHttpTransport(
        UGamePlatformOnlineClientSubsystem* InOnlineSubsystem);
    virtual ~FGamePlatformInventoryGatewayHttpTransport() override;

    /** 当前 Online（在线）子系统是否仍有可发送受保护请求的认证上下文。 */
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
