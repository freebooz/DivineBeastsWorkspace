#pragma once

// 平台客户端领域异步Port；调用方为LocalPlayer投影，传输不成为权威状态或资产所有者。
// 游戏线程调用与完成；Begin=false不执行Completion，true正常终态完成一次；CancelAll/退出抑制旧回调。
// 写请求取消只结束本地等待，服务器是否已提交须通过领域查询/对账确认。

#include "CoreMinimal.h"
#include "Types/GamePlatformCommerceUITypes.h"

using FGamePlatformCommerceCatalogCompletion =
    TFunction<void(
        FGamePlatformCommerceCatalogSnapshot,
        EGamePlatformCommerceError)>;

using FGamePlatformCommerceIntentCompletion =
    TFunction<void(
        FGamePlatformCommercePurchaseIntentView,
        EGamePlatformCommerceError)>;

using FGamePlatformCommerceOrderCompletion =
    TFunction<void(
        FGamePlatformCommerceOrderStatusView,
        EGamePlatformCommerceError)>;

class GAMEPLATFORMCOMMERCEUICLIENT_API IGamePlatformCommerceClientTransport
{
public:
    virtual ~IGamePlatformCommerceClientTransport() = default;

    /** 只取消本Port自有请求；不能取消同Online实例的其他领域请求或记录敏感凭据。 */
    virtual void CancelAllRequests() = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginGetCatalog(
        FGamePlatformCommerceCatalogCompletion Completion) = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginCreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId,
        FGamePlatformCommerceIntentCompletion Completion) = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginPurchase(
        const FString& PurchaseIntentId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginGetOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginSubmitReceipt(
        const FString& OrderId,
        const FString& Receipt,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    /** 必須提供有效完成函数；参数为领域身份/版本值，成功受理不表示后端已成功；结果及错误由Completion交付。 */
    virtual bool BeginReconcileOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;
};
