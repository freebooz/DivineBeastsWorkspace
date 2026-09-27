#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformCommerceClientTransport.h"

class FJsonObject;
class IHttpRequest;

class GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceGatewayHttpTransport final
    : public IGamePlatformCommerceClientTransport
    , public TSharedFromThis<
        FGamePlatformCommerceGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformCommerceGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    bool IsConfigured() const;

    virtual void CancelAllRequests() override;

    virtual bool BeginGetCatalog(
        FGamePlatformCommerceCatalogCompletion Completion) override;

    virtual bool BeginCreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId,
        FGamePlatformCommerceIntentCompletion Completion) override;

    virtual bool BeginPurchase(
        const FString& PurchaseIntentId,
        FGamePlatformCommerceOrderCompletion Completion) override;

    virtual bool BeginGetOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) override;

    virtual bool BeginSubmitReceipt(
        const FString& OrderId,
        const FString& Receipt,
        FGamePlatformCommerceOrderCompletion Completion) override;

    virtual bool BeginReconcileOrder(
        const FString& OrderId,
        FGamePlatformCommerceOrderCompletion Completion) override;

private:
    FString GatewayBaseUrl;
    FString AccessToken;

    FCriticalSection ActiveRequestsMutex;
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>>
        ActiveRequests;

    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<FJsonObject>& Body,
        TFunction<void(int32, const FString&)> Completion);

    void UnregisterRequest(
        const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request);

    static bool JsonToCatalog(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommerceCatalogSnapshot& OutCatalog);

    static bool JsonToIntent(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommercePurchaseIntentView& OutIntent);

    static bool JsonToOrder(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommerceOrderStatusView& OutOrder);

    static bool JsonToPrice(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformCommercePriceView& OutPrice,
        int64* OutTotalAmountMinor = nullptr);

    static EGamePlatformCommercePriceType ParsePriceType(
        const FString& Value);

    static FString FormatDisplayPrice(
        const FGamePlatformCommercePriceView& Price);

    static bool ParseIso8601(
        const TSharedPtr<FJsonObject>& Json,
        const TCHAR* Field,
        FDateTime& OutValue,
        bool bRequired);

    static EGamePlatformCommerceError MapHttpError(
        int32 StatusCode,
        const FString& Body);
};
