#pragma once

// 平台客户端领域传输：只转换业务JSON；Online实例独占认证、HTTP、刷新和预算。
// 游戏线程创建/调用/销毁；仅持有Online弱引用及自己签发的请求句柄，退出幂等取消。

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformLiveOpsClientTransport.h"

class FJsonObject;
class UGamePlatformOnlineClientSubsystem;

class GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsGatewayHttpTransport final
    : public IGamePlatformLiveOpsClientTransport
    , public TSharedFromThis<
        FGamePlatformLiveOpsGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    /** 同实例Online完成认证后注入；nullptr表示未配置，Begin返回false且不执行回调。 */
    explicit FGamePlatformLiveOpsGatewayHttpTransport(UGamePlatformOnlineClientSubsystem* InOnlineSubsystem);
    /** 销毁时只取消本领域拥有的请求；不退出Online或撤销其他领域请求。 */
    virtual ~FGamePlatformLiveOpsGatewayHttpTransport() override;
    /** 旧签名仅保留源码迁移兼容，不保存URL/Token，始终未配置；请改用Online构造。 */
    UE_DEPRECATED(5.8, "Inject UGamePlatformOnlineClientSubsystem instead of URL/token")
    FGamePlatformLiveOpsGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    /** 仅Online处于Authenticated/Refreshing且实例有效时为true，不读取认证票据。 */
    bool IsConfigured() const;

    /** 幂等失效本领域回调代次并逐个取消自有Online句柄；后端已经提交的写命令不回滚。 */
    virtual void CancelAllRequests() override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginGetCatalog(
        FGamePlatformLiveOpsCatalogCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginGetPlayerState(
        FGamePlatformLiveOpsPlayerStateCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginClaimSignIn(
        const FGuid& ClaimOperationId,
        FName CampaignId,
        FGamePlatformLiveOpsClaimCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginQueryClaimOperation(
        const FGuid& ClaimOperationId,
        FGamePlatformLiveOpsClaimCompletion Completion) override;

private:
    struct FRuntime;
    TUniquePtr<FRuntime> Runtime;

    /** 有界受理最多8个领域请求；JSON序列化失败不发请求，认证/总截止/刷新由Online负责。 */
    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<FJsonObject>& Body,
        TFunction<void(int32, const FString&, EGamePlatformLiveOpsError)> Completion);

    /** 终态先撤销自己的句柄记录；回调不强持有传输对象。 */
    void UnregisterRequest(const FGuid& RequestId);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToCatalog(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsCatalogSnapshot& OutCatalog);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToPlayerState(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsPlayerState& OutState);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToClaim(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsClaimResult& OutClaim);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool ParseTimeWindow(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsTimeWindow& OutWindow);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool ParseIsoTime(
        const TSharedPtr<FJsonObject>& Json,
        const TCHAR* Field,
        FDateTime& OutTime,
        bool bRequired);

    /** 将后端状态/错误码映射为领域错误；认证/取消/截止错误已在Online响应层优先处理。 */
    static EGamePlatformLiveOpsError MapHttpError(
        int32 StatusCode,
        const FString& Body);
};
