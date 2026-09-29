#pragma once

#include "CoreMinimal.h"
#include "GamePlatformOnlineClientSubsystem.h"

class IHttpRequest;

/**
 * FGamePlatformGatewayAuthProvider（平台Gateway认证/受保护请求传输）。
 *
 * 只处理 GamePlatform 公共 Gateway 契约，不包含神兽联盟项目语义。
 * 原始 AccessToken/RefreshToken 仅存在本类私有内存；调用方永远拿不到 Authorization 值。
 */
class FGamePlatformGatewayAuthProvider final
    : public IGamePlatformOnlineAuthProvider,
      public TSharedFromThis<FGamePlatformGatewayAuthProvider>
{
public:
    FGamePlatformGatewayAuthProvider();
    virtual ~FGamePlatformGatewayAuthProvider() override;

    virtual FGamePlatformResult Configure(
        const FGamePlatformOnlineConfiguration& Configuration) override;
    virtual void TryAutoLogin(FGamePlatformAuthCompletion Completion) override;
    virtual void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password,
        FGamePlatformAuthCompletion Completion) override;
    virtual void Refresh(FGamePlatformAuthCompletion Completion) override;
    virtual void Logout(FGamePlatformAuthLogoutCompletion Completion) override;
    virtual void SendUnauthenticatedRequest(
        const FGuid& RequestId,
        const FGamePlatformAuthenticatedRequest& Request,
        FGamePlatformAuthenticatedCompletion Completion) override;
    virtual void SendAuthenticatedRequest(
        const FGuid& RequestId,
        const FGamePlatformAuthenticatedRequest& Request,
        FGamePlatformAuthenticatedCompletion Completion) override;
    virtual void CancelRequest(const FGuid& RequestId) override;
    virtual void InvalidateAuthenticationOperation() override;
    virtual bool ApplyAuthorization(IHttpRequest& Request) const override;
    virtual void CancelAll() override;

private:
    struct FRawResponse
    {
        int32 HttpCode = 0;
        FString Body;
        bool bTransportSuccess = false;
        bool bMayHaveReachedServer = false;
        bool bResponseTooLarge = false;
        double RetryAfterSeconds = 0.0;
    };

    using FRawCompletion = TFunction<void(FRawResponse)>;

    void SendRequest(
        const FGuid& RequestId,
        const FString& Verb,
        const FString& RelativePath,
        const FString& Body,
        const FString& Authorization,
        const FString& IdempotencyKey,
        FRawCompletion Completion);

    bool ParseAndCommitLoginResponse(
        const FString& ResponseText,
        FGamePlatformAuthProviderResult& OutResult);
    void ClearTokens();

    static bool IsRelativePathSafe(const FString& RelativePath);
    static EGamePlatformAuthError MapAuthError(
        int32 HttpCode,
        bool bTransportSuccess,
        bool bRefreshOperation,
        bool bResponseTooLarge);
    static EGamePlatformAuthError MapRequestError(const FRawResponse& Response);

    FGamePlatformOnlineConfiguration Configuration;
    FString AccessToken;
    FString RefreshToken;
    FString AccountId;
    FString SessionId;
    FDateTime AccessExpiresAt;
    FDateTime RefreshExpiresAt;
    uint64 OperationGeneration = 0;

    TMap<FGuid, TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;
    bool bConfigured = false;
};
