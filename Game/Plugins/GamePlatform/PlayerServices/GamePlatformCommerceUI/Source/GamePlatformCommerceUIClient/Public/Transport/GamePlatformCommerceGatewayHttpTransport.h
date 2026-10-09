#pragma once

// 平台客户端领域传输：只转换业务JSON；Online实例独占认证、HTTP、刷新和预算。
// 游戏线程创建/调用/销毁；仅持有Online弱引用及自己签发的请求句柄，退出幂等取消。

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformCommerceClientTransport.h"

class FJsonObject;
class UGamePlatformOnlineClientSubsystem;

class GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceGatewayHttpTransport final
    : public IGamePlatformCommerceClientTransport
    , public TSharedFromThis<
        FGamePlatformCommerceGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    /** 同实例Online完成认证后注入；nullptr表示未配置，Begin返回false且不执行回调。 */
    explicit FGamePlatformCommerceGatewayHttpTransport(UGamePlatformOnlineClientSubsystem* InOnlineSubsystem);
    /** 销毁时只取消本领域拥有的请求；不退出Online或撤销其他领域请求。 */
    virtual ~FGamePlatformCommerceGatewayHttpTransport() override;
    /** 旧签名仅保留源码迁移兼容，不保存URL/Token，始终未配置；请改用Online构造。 */
    UE_DEPRECATED(5.8, "Inject UGamePlatformOnlineClientSubsystem instead of URL/token")
    FGamePlatformCommerceGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    /** 仅Online处于Authenticated/Refreshing且实例有效时为true，不读取认证票据。 */
    bool IsConfigured() const;

    /** 幂等失效本领域回调代次并逐个取消自有Online句柄；后端已经提交的写命令不回滚。 */
    virtual void CancelAllRequests() override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginGetCatalog(
        FGamePlatformCommerceCatalogCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginCreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId,
        FGamePlatformCommerceIntentCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginPurchase(
        const FString& PurchaseIntentId,
        FGamePlatformCommerceOrderCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginGetOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginSubmitReceipt(
        const FString& OrderId,
        const FString& Receipt,
        FGamePlatformCommerceOrderCompletion Completion) override;

    /** 仅游戏线程；参数沿领域Port校验；false未受理，true异步完成一次，取消/退出后旧回调被抑制。 */
    virtual bool BeginReconcileOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
    // 测试仅访问实际生产解析入口，不复制解析实现或绕过网络权限。
    friend class FGamePlatformCommerceJsonValidationTest;
#endif
    struct FRuntime;
    TUniquePtr<FRuntime> Runtime;

    /** 有界受理最多8个领域请求；JSON序列化失败不发请求，认证/总截止/刷新由Online负责。 */
    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<FJsonObject>& Body,
        TFunction<void(int32, const FString&, EGamePlatformCommerceError)> Completion);

    /** 终态先撤销自己的句柄记录；回调不强持有传输对象。 */
    void UnregisterRequest(const FGuid& RequestId);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToCatalog(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommerceCatalogSnapshot& OutCatalog);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToIntent(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommercePurchaseIntentView& OutIntent);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToOrder(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommerceOrderStatusView& OutOrder);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool JsonToPrice(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommercePriceView& OutPrice,
        int64* OutTotalAmountMinor = nullptr);

    static EGamePlatformCommercePriceType ParsePriceType(
        const FString& Value);

    static FString FormatDisplayPrice(
        const FGamePlatformCommercePriceView& Price);

    /** 将受限响应解码为领域纯值；缺必填字段/非法类型返回false，不授予业务权威。 */
    static bool ParseIso8601(
        const TSharedPtr<FJsonObject>& Json,
        const TCHAR* Field,
        FDateTime& OutValue,
        bool bRequired);

    /** 将后端状态/错误码映射为领域错误；认证/取消/截止错误已在Online响应层优先处理。 */
    static EGamePlatformCommerceError MapHttpError(
        int32 StatusCode,
        const FString& Body);
};
