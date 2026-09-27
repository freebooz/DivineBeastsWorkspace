#pragma once

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

    virtual void CancelAllRequests() = 0;

    virtual bool BeginGetCatalog(
        FGamePlatformCommerceCatalogCompletion Completion) = 0;

    virtual bool BeginCreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId,
        FGamePlatformCommerceIntentCompletion Completion) = 0;

    virtual bool BeginPurchase(
        const FString& PurchaseIntentId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    virtual bool BeginGetOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    virtual bool BeginSubmitReceipt(
        const FString& OrderId,
        const FString& Receipt,
        FGamePlatformCommerceOrderCompletion Completion) = 0;

    virtual bool BeginReconcileOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) = 0;
};
