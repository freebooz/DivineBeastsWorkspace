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
    None,
    CatalogUnavailable,
    ProductNotFound,
    OfferNotFound,
    OfferNotActive,
    OfferNotEligible,
    OfferRevisionMismatch,
    PriceChanged,
    InvalidQuantity,
    PurchaseLimitReached,
    PurchaseIntentExpired,
    OrderNotFound,
    InvalidOrderState,
    PaymentPending,
    PaymentFailed,
    PaymentVerificationFailed,
    ReceiptReplay,
    ProviderUnavailable,
    FulfillmentPending,
    FulfillmentFailed,
    UnsupportedRewardType,
    SoftCurrencyUnavailable,
    RefundUnsupported,
    OutcomeUnknown,
    BackendUnavailable,
    Unauthorized,
    Cancelled,
    TimedOut,
    InvalidResponse
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

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName PriceId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    EGamePlatformCommercePriceType PriceType =
        EGamePlatformCommercePriceType::Unknown;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString CurrencyCode;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString CurrencyId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 UnitAmountMinor = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString DisplayText;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceProductCardView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductType = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString NameKey;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString DescriptionKey;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName PresentationMetadataId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceOfferView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OfferRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime StartsAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bHasEnd = false;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime EndsAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName LiveOpsEventId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bActiveProjection = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceCatalogSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 CatalogRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime ServerTimeUtc;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    TArray<FGamePlatformCommerceProductCardView> Products;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    TArray<FGamePlatformCommerceOfferView> Offers;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommercePurchaseIntentView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PurchaseIntentId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 CatalogRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OfferRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 TotalAmountMinor = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FDateTime ExpiresAtUtc;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceProviderFlow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString ProviderName;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString SessionId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString ClientToken;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bNonProduction = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommerceOrderStatusView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString OrderId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PurchaseIntentId;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName ProductId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FName OfferId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString OrderState;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString PaymentState;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FString FulfillmentState;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 OrderRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePriceView Price;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    int64 TotalAmountMinor = 0;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommerceProviderFlow ProviderFlow;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPurchaseSucceeded = false;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPaymentConfirmedRewardPending = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMMERCEUICLIENT_API FGamePlatformCommercePurchaseConfirmationView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    FGamePlatformCommercePurchaseIntentView Intent;

    UPROPERTY(BlueprintReadOnly, Category="Commerce")
    bool bPriceIsAuthoritativeSnapshot = true;
};
