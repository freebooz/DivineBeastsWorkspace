// 本地玩家商店只读投影；金额为明确币种最小单位整数，业务真源/付款验证/发奖均在后端；账号清空撤销短期状态。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformCommerceUITypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformCommerceClientState : uint8
{
    Idle,
    LoadingCatalog,
    Browsing,
    CreatingIntent,
    Confirming,
    AwaitingProvider,
    AwaitingVerification,
    AwaitingFulfillment,
    Succeeded,
    Failed,
    Reconciling
};

UENUM(BlueprintType)
enum class EGamePlatformCommerceError : uint8
{
    None, // 当前操作无错误，不表示客户端获得权威写权限
    CatalogUnavailable, // 运营/商店目录不可用
    ProductNotFound, // 后端产品身份不存在
    OfferNotFound, // 销售方案身份不存在
    OfferNotActive, // 销售方案不在权威有效窗口
    OfferNotEligible, // 玩家不满足后端购买资格
    OfferRevisionMismatch, // 购买使用的销售方案版本已失效
    PriceChanged, // 后端当前价格不同，必须重建/确认意向
    InvalidQuantity, // 数量非正/超过源物品或业务范围
    PurchaseLimitReached, // 后端购买限额已达到
    PurchaseIntentExpired, // 后端购买意向已过期，不继续支付
    OrderNotFound, // 原订单身份不存在，不能伪造成功
    InvalidOrderState, // 订单状态不允许当前命令
    PaymentPending, // 后端尚未确认支付终态
    PaymentFailed, // 后端确认支付失败
    PaymentVerificationFailed, // 后端支付凭据校验失败
    ReceiptReplay, // 凭据已用于原记录，不能重复到账
    ProviderUnavailable, // 真实支付Provider未配置/不可用
    FulfillmentPending, // 付款可能已确认但奖励履约尚未完成
    FulfillmentFailed, // 后端奖励履约失败，客户端不能虚构奖品
    UnsupportedRewardType, // 后端不支持该奖励类型
    SoftCurrencyUnavailable, // 软货币权威钱包/余额不可用
    RefundUnsupported, // 当前后端不提供退款能力，客户端不能直接改余额
    OutcomeUnknown, // 请求可能已提交，必须按原操作/订单身份查询对账
    BackendUnavailable, // 领域传输/后端不可用，保留可读投影
    Unauthorized, // 认证失效或调用方权限不足
    Cancelled, // 仅本地等待取消，不能回滚已提交的权威事务
    TimedOut, // 等待截止时间已到，是否提交依终态与原操作对账
    InvalidResponse // 响应结构/身份/版本无法验证，不能替换旧快照
};

UENUM(BlueprintType)
enum class EGamePlatformCommercePriceType : uint8
{
    Unknown,
    RealMoney,
    SoftCurrency,
    Free
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommercePriceView
{
    GENERATED_BODY()

    /** 后端价格身份；None未提供，不能用于确认权威价格。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName PriceId = NAME_None;

    /** 后端价格类别；Unknown未解析，不允许从显示文字推断货币。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    EGamePlatformCommercePriceType PriceType =
        EGamePlatformCommercePriceType::Unknown;

    /** 真实货币代码，金额以该币种最小单位整数表示；空值不代表免费。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString CurrencyCode;

    /** 软货币稳定身份；空值未指定，客户端不修改钱包余额。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString CurrencyId;

    /** 单件价格的货币最小单位整数；0可表示免费，不能使用浮点。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 UnitAmountMinor = 0;

    /** 后端/本地化价格展示文本；仅展示，不能作为支付或扣款依据。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString DisplayText;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceProductCardView
{
    GENERATED_BODY()

    /** 后端稳定产品身份；None无效，不是资源挂载路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    /** 产品类别稳定语义；None未声明，客户端不据此自行发奖。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductType = NAME_None;

    /** 产品名称本地化键；空值未提供，由界面执行可读回退。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString NameKey;

    /** 产品说明本地化键；空值未提供，不作为机器错误码。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString DescriptionKey;

    /** 可选产品表现元数据身份；缺失不改变产品权威与支付结果。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName PresentationMetadataId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceOfferView
{
    GENERATED_BODY()

    /** 后端稳定销售方案身份；None无效，创建意向必须提供。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    /** 后端稳定产品身份；None无效，不是资源挂载路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    /** 后端销售方案单调版本；0未知，价格变化由后端拒绝/重建意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OfferRevision = 0;

    /** 后端结构化价格投影；支付确认以当前有效意向/订单为准。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    /** 活动/方案开始UTC时刻；与服务器时间比较，不使用本地时区。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime StartsAtUtc;

    /** 是否声明结束UTC；false表示开放结束，不读取EndsAtUtc。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bHasEnd = false;

    /** 可选结束UTC时刻，仅bHasEnd=true时参与展示。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime EndsAtUtc;

    /** 可选运营事件身份；None无绑定，不使客户端拥有运营权威。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName LiveOpsEventId = NAME_None;

    /** 后端方案可见性投影；默认false，仍须后端确认购买资格。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bActiveProjection = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceCatalogSnapshot
{
    GENERATED_BODY()

    /** 后端目录单调版本；0未加载，不能据此生成有效意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 CatalogRevision = 0;

    /** 后端生成快照UTC时刻；无有效时间的响应拒绝或由业务解析报告错误。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime GeneratedAtUtc;

    /** 后端UTC时间样本；仅派生展示窗口，不作为客户端发奖依据。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime ServerTimeUtc;

    /** 后端发布的产品卡集合；空集合合法，不生成占位商品。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    TArray<FGamePlatformCommerceProductCardView> Products;

    /** 后端发布的销售方案集合；空集合合法，不伪造库存与价格。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    TArray<FGamePlatformCommerceOfferView> Offers;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommercePurchaseIntentView
{
    GENERATED_BODY()

    /** 后端一次购买意向身份；空值无效，不能重复消费已失效意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PurchaseIntentId;

    /** 后端回显的创建意向幂等请求身份；用于核对同一次业务命令。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString RequestId;

    /** 后端稳定产品身份；None无效，不是资源挂载路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    /** 后端稳定销售方案身份；None无效，创建意向必须提供。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    /** 物品单位整数数量；有效实例/购买请求必须大于0，不使用浮点或本地授予。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int32 Quantity = 0;

    /** 后端目录单调版本；0未加载，不能据此生成有效意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 CatalogRevision = 0;

    /** 后端销售方案单调版本；0未知，价格变化由后端拒绝/重建意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OfferRevision = 0;

    /** 后端结构化价格投影；支付确认以当前有效意向/订单为准。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    /** 总价的货币最小单位整数，币种由Price给出；0不代表付款验证成功。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 TotalAmountMinor = 0;

    /** 意向失效UTC时刻；客户端只提示，最终是否可用由后端裁定。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime ExpiresAtUtc;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceProviderFlow
{
    GENERATED_BODY()

    /** 支付Provider稳定名称；空值尚未建立支付流程。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString ProviderName;

    /** 支付Provider会话身份；仅本次受控支付流程使用，不写日志/配置。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString SessionId;

    /** Provider明确允许客户端使用的短期材料；不记录日志/遥测，不当作账号认证令牌。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString ClientToken;

    /** 是否明确属于非生产支付环境；默认false，测试环境不代表真实充值成功。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bNonProduction = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceOrderStatusView
{
    GENERATED_BODY()

    /** 后端订单唯一身份；空值无订单，恢复必须查询原订单。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString OrderId;

    /** 后端一次购买意向身份；空值无效，不能重复消费已失效意向。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PurchaseIntentId;

    /** 后端稳定产品身份；None无效，不是资源挂载路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    /** 后端稳定销售方案身份；None无效，创建意向必须提供。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    /** 物品单位整数数量；有效实例/购买请求必须大于0，不使用浮点或本地授予。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int32 Quantity = 0;

    /** 后端订单机器状态；只有fulfilled配合支付/履约状态确认成功。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString OrderState;

    /** 后端支付验证状态；Provider本地SDK成功不能替代confirmed。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PaymentState;

    /** 后端发奖状态；付款确认而未fulfilled时仍显示待发奖。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString FulfillmentState;

    /** 后端订单单调版本；0未知，状态终态由后端返回。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OrderRevision = 0;

    /** 后端结构化价格投影；支付确认以当前有效意向/订单为准。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    /** 总价的货币最小单位整数，币种由Price给出；0不代表付款验证成功。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 TotalAmountMinor = 0;

    /** 仅用于受控支付SDK接续的短期投影，账号清空后撤销。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommerceProviderFlow ProviderFlow;

    /** 客户端按后端三项完成状态派生的显示标志；默认false，不执行发奖。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPurchaseSucceeded = false;

    /** 已后端确认付款但奖励待履约的显示标志；默认false，不能重复付款解决。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPaymentConfirmedRewardPending = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommercePurchaseConfirmationView
{
    GENERATED_BODY()

    /** 当前后端购买意向只读投影；空值没有确认对象。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePurchaseIntentView Intent;

    /** 价格是后端意向生成时的快照提示；不保证意向未过期，仍须后端确认。 */
    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPriceIsAuthoritativeSnapshot = true;
};
