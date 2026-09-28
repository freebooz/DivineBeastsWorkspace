#pragma once

#include "CoreMinimal.h"
#include "GamePlatformOnlineClientSubsystem.h"

class IHttpRequest;
class IHttpResponse;

/**
 * FDivineBeastsGatewayAuthProvider（神兽联盟 Gateway 认证提供者）。
 *
 * 第一阶段客户端登录的项目装配实现：
 * - 使用 Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml 已存在的
 *   /v1/auth/login、/v1/auth/refresh、/v1/auth/logout 契约；
 * - 只负责把平台 IGamePlatformOnlineAuthProvider（认证提供者接口）接到真实 Gateway；
 * - AccessToken / RefreshToken 只保存在本对象私有内存，不进入 UObject、ViewState、日志或遥测；
 * - Refresh（刷新）绝不自动重试；轮换响应不确定时清空本地令牌并要求重新认证；
 * - Logout（退出）先完成本地令牌清理，再尽力通知服务端，不因远端失败恢复旧认证。
 *
 * 当前没有安全持久凭据存储，因此 TryAutoLogin（自动登录）明确返回 AuthExpired，
 * 不从配置、命令行或普通磁盘文件读取玩家密码/RefreshToken。
 */
class FDivineBeastsGatewayAuthProvider final
    : public IGamePlatformOnlineAuthProvider,
      public TSharedFromThis<FDivineBeastsGatewayAuthProvider>
{
public:
    FDivineBeastsGatewayAuthProvider();
    virtual ~FDivineBeastsGatewayAuthProvider() override;

    virtual void TryAutoLogin(FGamePlatformAuthCompletion Completion) override;

    virtual void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password,
        FGamePlatformAuthCompletion Completion) override;

    virtual void Refresh(FGamePlatformAuthCompletion Completion) override;

    virtual void Logout(TFunction<void()> Completion) override;

    /** 仅在受保护请求发起瞬间生成 Authorization Header，不对外暴露原始 AccessToken。 */
    virtual FString GetAuthorizationHeaderValue() const override;

private:
    using FRawHttpCompletion = TFunction<void(
        int32 HttpCode,
        const FString& ResponseText,
        bool bTransportSuccess)>;

    /** 发送一次 JSON POST；本层不执行自动重试。 */
    void SendJsonPost(
        const FString& RelativePath,
        const TSharedRef<class FJsonObject>& Body,
        FRawHttpCompletion Completion);

    /** 解析 Login/Refresh 的统一 LoginResponse，并在全部字段合法后一次性提交新令牌。 */
    bool ParseAndCommitLoginResponse(
        const FString& ResponseText,
        FString& OutPlayerId);

    /** 取消本 Provider 持有的网络请求并使全部旧回调失效。 */
    void CancelAll();

    /** Gateway 地址必须是 HTTPS；非 Shipping 仅额外允许字面回环 HTTP。 */
    static bool IsAllowedGatewayBaseUrl(const FString& Value);

    /** 把 HTTP/传输失败收敛为平台认证错误，不把原始服务端正文暴露到 UI。 */
    static EGamePlatformAuthError MapAuthError(
        int32 HttpCode,
        bool bTransportSuccess,
        bool bRefreshOperation);

    FString GatewayBaseUrl;
    FString GameId = TEXT("divine-beasts");
    FString ClientVersion = TEXT("0.1.0");

    /** 私有认证材料；严禁写日志、配置、UProperty或诊断。 */
    FString AccessToken;
    FString RefreshToken;
    FString AccountId;

    /** 本 Provider 的异步代次；登录/刷新/退出/销毁均推进，用于拒绝迟到网络回调。 */
    uint64 OperationGeneration = 0;

    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;
};
