#include "Transport/GamePlatformCommerceGatewayHttpTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString GuidString(const FGuid& Value)
{
    return Value.IsValid()
        ? Value.ToString(EGuidFormats::DigitsWithHyphensLower)
        : FString();
}

bool NumberToInt64(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    int64& OutValue)
{
    double Number = 0.0;
    if (!Json.IsValid() ||
        !Json->TryGetNumberField(Field, Number) ||
        !FMath::IsFinite(Number) ||
        Number < static_cast<double>(MIN_int64) ||
        Number > static_cast<double>(MAX_int64))
    {
        return false;
    }

    OutValue = static_cast<int64>(Number);
    return true;
}
}

FGamePlatformCommerceGatewayHttpTransport::
FGamePlatformCommerceGatewayHttpTransport(
    FString InGatewayBaseUrl,
    FString InAccessToken)
    : GatewayBaseUrl(MoveTemp(InGatewayBaseUrl))
    , AccessToken(MoveTemp(InAccessToken))
{
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool FGamePlatformCommerceGatewayHttpTransport::IsConfigured() const
{
    return !GatewayBaseUrl.IsEmpty() &&
           !AccessToken.IsEmpty();
}

void FGamePlatformCommerceGatewayHttpTransport::CancelAllRequests()
{
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> Requests;
    {
        FScopeLock Lock(&ActiveRequestsMutex);
        Requests = ActiveRequests;
        ActiveRequests.Reset();
    }

    for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request :
         Requests)
    {
        if (Request.IsValid())
        {
            Request->CancelRequest();
        }
    }
}

bool FGamePlatformCommerceGatewayHttpTransport::StartJsonRequest(
    const FString& Verb,
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    TFunction<void(int32, const FString&)> Completion)
{
    if (!IsConfigured() ||
        Verb.IsEmpty() ||
        Path.IsEmpty() ||
        !Completion)
    {
        return false;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> RequestPtr = Request;

    Request->SetURL(GatewayBaseUrl + Path);
    Request->SetVerb(Verb);
    Request->SetHeader(
        TEXT("Authorization"),
        FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

    if (Body.IsValid())
    {
        FString Payload;
        TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Payload);

        if (!FJsonSerializer::Serialize(
                Body.ToSharedRef(),
                Writer))
        {
            return false;
        }

        Request->SetHeader(
            TEXT("Content-Type"),
            TEXT("application/json"));
        Request->SetContentAsString(Payload);
    }

    {
        FScopeLock Lock(&ActiveRequestsMutex);
        constexpr int32 MaxConcurrentHttpRequests = 8;
        if (ActiveRequests.Num() >= MaxConcurrentHttpRequests)
        {
            return false;
        }
        ActiveRequests.Add(RequestPtr);
    }

    TSharedRef<
        FGamePlatformCommerceGatewayHttpTransport,
        ESPMode::ThreadSafe> Self = AsShared();

    Request->OnProcessRequestComplete().BindLambda(
        [Self,
         RequestPtr,
         Completion = MoveTemp(Completion)](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bConnectedSuccessfully) mutable
        {
            const int32 StatusCode =
                bConnectedSuccessfully && Response.IsValid()
                    ? Response->GetResponseCode()
                    : 0;

            const FString ResponseBody =
                Response.IsValid()
                    ? Response->GetContentAsString()
                    : FString();

            Self->UnregisterRequest(RequestPtr);

            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Completion),
                 StatusCode,
                 ResponseBody]() mutable
                {
                    Completion(StatusCode, ResponseBody);
                });
        });

    const bool bStarted = Request->ProcessRequest();
    if (!bStarted)
    {
        UnregisterRequest(RequestPtr);
    }
    return bStarted;
}

void FGamePlatformCommerceGatewayHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&ActiveRequestsMutex);
    ActiveRequests.Remove(Request);
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginGetCatalog(
    FGamePlatformCommerceCatalogCompletion Completion)
{
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/commerce/catalog"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, Body));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformCommerceCatalogSnapshot Catalog;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToCatalog(Json, Catalog))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Catalog),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginCreatePurchaseIntent(
    FName OfferId,
    int32 Quantity,
    const FGuid& RequestId,
    FGamePlatformCommerceIntentCompletion Completion)
{
    if (OfferId.IsNone() ||
        Quantity <= 0 ||
        !RequestId.IsValid())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("offer_id"), OfferId.ToString());
    Body->SetNumberField(TEXT("quantity"), Quantity);
    Body->SetStringField(TEXT("request_id"), GuidString(RequestId));

    return StartJsonRequest(
        TEXT("POST"),
        TEXT("/v1/commerce/intents"),
        Body,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformCommercePurchaseIntentView Intent;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToIntent(Json, Intent))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Intent),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginPurchase(
    const FString& PurchaseIntentId,
    FGamePlatformCommerceOrderCompletion Completion)
{
    if (PurchaseIntentId.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("purchase_intent_id"),
        PurchaseIntentId);

    return StartJsonRequest(
        TEXT("POST"),
        TEXT("/v1/commerce/orders"),
        Body,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformCommerceOrderStatusView Order;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToOrder(Json, Order))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Order),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginGetOrder(
    const FString& OrderId,
    FGamePlatformCommerceOrderCompletion Completion)
{
    if (OrderId.IsEmpty())
    {
        return false;
    }

    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/commerce/orders/") +
            FGenericPlatformHttp::UrlEncode(OrderId),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformCommerceOrderStatusView Order;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToOrder(Json, Order))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Order),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginSubmitReceipt(
    const FString& OrderId,
    const FString& Receipt,
    FGamePlatformCommerceOrderCompletion Completion)
{
    if (OrderId.IsEmpty() || Receipt.IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("receipt"), Receipt);

    return StartJsonRequest(
        TEXT("POST"),
        TEXT("/v1/commerce/orders/") +
            FGenericPlatformHttp::UrlEncode(OrderId) +
            TEXT("/receipt"),
        Body,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformCommerceOrderStatusView Order;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToOrder(Json, Order))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Order),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::BeginReconcileOrder(
    const FString& OrderId,
    FGamePlatformCommerceOrderCompletion Completion)
{
    if (OrderId.IsEmpty())
    {
        return false;
    }

    return StartJsonRequest(
        TEXT("POST"),
        TEXT("/v1/commerce/orders/") +
            FGenericPlatformHttp::UrlEncode(OrderId) +
            TEXT("/reconcile"),
        MakeShared<FJsonObject>(),
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformCommerceOrderStatusView Order;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToOrder(Json, Order))
            {
                Completion(
                    {},
                    EGamePlatformCommerceError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Order),
                EGamePlatformCommerceError::None);
        });
}

bool FGamePlatformCommerceGatewayHttpTransport::JsonToCatalog(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformCommerceCatalogSnapshot& OutCatalog)
{
    if (!Json.IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* CatalogJson = nullptr;
    int64 Revision = 0;

    if (!Json->TryGetObjectField(TEXT("catalog"), CatalogJson) ||
        !CatalogJson ||
        !(*CatalogJson).IsValid() ||
        !NumberToInt64(
            *CatalogJson,
            TEXT("catalog_revision"),
            Revision) ||
        Revision < 1 ||
        !ParseIso8601(
            *CatalogJson,
            TEXT("generated_at"),
            OutCatalog.GeneratedAtUtc,
            true) ||
        !ParseIso8601(
            Json,
            TEXT("server_time_utc"),
            OutCatalog.ServerTimeUtc,
            true))
    {
        return false;
    }

    OutCatalog.CatalogRevision = Revision;

    TMap<FName, FGamePlatformCommercePriceView> Prices;
    const TArray<TSharedPtr<FJsonValue>>* PriceValues = nullptr;

    if ((*CatalogJson)->TryGetArrayField(
            TEXT("prices"),
            PriceValues) &&
        PriceValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *PriceValues)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;

            FGamePlatformCommercePriceView Price;
            if (!Object.IsValid() ||
                !JsonToPrice(Object, Price))
            {
                return false;
            }

            Prices.Add(Price.PriceId, Price);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* ProductValues = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("products"),
            ProductValues) &&
        ProductValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ProductValues)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString ProductId;
            FString ProductType;
            FString NameKey;
            FString DescriptionKey;
            FString Presentation;

            if (!Object->TryGetStringField(
                    TEXT("product_id"),
                    ProductId) ||
                !Object->TryGetStringField(
                    TEXT("product_type"),
                    ProductType) ||
                !Object->TryGetStringField(
                    TEXT("name_key"),
                    NameKey) ||
                !Object->TryGetStringField(
                    TEXT("description_key"),
                    DescriptionKey))
            {
                return false;
            }

            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            FGamePlatformCommerceProductCardView Product;
            Product.ProductId = FName(*ProductId);
            Product.ProductType = FName(*ProductType);
            Product.NameKey = NameKey;
            Product.DescriptionKey = DescriptionKey;
            Product.PresentationMetadataId = FName(*Presentation);
            OutCatalog.Products.Add(MoveTemp(Product));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* OfferValues = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("offers"),
            OfferValues) &&
        OfferValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *OfferValues)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString OfferId;
            FString ProductId;
            FString PriceId;
            FString LiveOpsEventId;
            int64 OfferRevision = 0;

            FGamePlatformCommerceOfferView Offer;

            if (!Object->TryGetStringField(TEXT("offer_id"), OfferId) ||
                !Object->TryGetStringField(TEXT("product_id"), ProductId) ||
                !Object->TryGetStringField(TEXT("price_id"), PriceId) ||
                !NumberToInt64(
                    Object,
                    TEXT("offer_revision"),
                    OfferRevision) ||
                !ParseIso8601(
                    Object,
                    TEXT("starts_at_utc"),
                    Offer.StartsAtUtc,
                    true))
            {
                return false;
            }

            FString EndsAt;
            if (Object->TryGetStringField(TEXT("ends_at_utc"), EndsAt) &&
                !EndsAt.IsEmpty())
            {
                if (!FDateTime::ParseIso8601(
                        *EndsAt,
                        Offer.EndsAtUtc))
                {
                    return false;
                }
                Offer.bHasEnd = true;
            }

            Object->TryGetStringField(
                TEXT("liveops_event_id"),
                LiveOpsEventId);

            Offer.OfferId = FName(*OfferId);
            Offer.ProductId = FName(*ProductId);
            Offer.OfferRevision = OfferRevision;
            Offer.LiveOpsEventId = FName(*LiveOpsEventId);

            const FGamePlatformCommercePriceView* Price =
                Prices.Find(FName(*PriceId));
            if (!Price)
            {
                return false;
            }
            Offer.Price = *Price;

            const FDateTime Now = OutCatalog.ServerTimeUtc;
            Offer.bActiveProjection =
                Now >= Offer.StartsAtUtc &&
                (!Offer.bHasEnd || Now < Offer.EndsAtUtc);

            OutCatalog.Offers.Add(MoveTemp(Offer));
        }
    }

    return true;
}

bool FGamePlatformCommerceGatewayHttpTransport::JsonToIntent(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformCommercePurchaseIntentView& OutIntent)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString ProductId;
    FString OfferId;
    double Quantity = 0.0;
    int64 CatalogRevision = 0;
    int64 OfferRevision = 0;

    const TSharedPtr<FJsonObject>* PriceJson = nullptr;

    if (!Json->TryGetStringField(
            TEXT("purchase_intent_id"),
            OutIntent.PurchaseIntentId) ||
        !Json->TryGetStringField(
            TEXT("request_id"),
            OutIntent.RequestId) ||
        !Json->TryGetStringField(
            TEXT("product_id"),
            ProductId) ||
        !Json->TryGetStringField(
            TEXT("offer_id"),
            OfferId) ||
        !Json->TryGetNumberField(
            TEXT("quantity"),
            Quantity) ||
        !NumberToInt64(
            Json,
            TEXT("catalog_revision"),
            CatalogRevision) ||
        !NumberToInt64(
            Json,
            TEXT("offer_revision"),
            OfferRevision) ||
        !Json->TryGetObjectField(
            TEXT("price_snapshot"),
            PriceJson) ||
        !PriceJson ||
        !JsonToPrice(
            *PriceJson,
            OutIntent.Price,
            &OutIntent.TotalAmountMinor) ||
        !ParseIso8601(
            Json,
            TEXT("expires_at"),
            OutIntent.ExpiresAtUtc,
            true))
    {
        return false;
    }

    OutIntent.ProductId = FName(*ProductId);
    OutIntent.OfferId = FName(*OfferId);
    OutIntent.Quantity = static_cast<int32>(Quantity);
    OutIntent.CatalogRevision = CatalogRevision;
    OutIntent.OfferRevision = OfferRevision;

    return !OutIntent.PurchaseIntentId.IsEmpty() &&
           OutIntent.Quantity > 0;
}

bool FGamePlatformCommerceGatewayHttpTransport::JsonToOrder(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformCommerceOrderStatusView& OutOrder)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString ProductId;
    FString OfferId;
    double Quantity = 0.0;
    int64 OrderRevision = 0;
    const TSharedPtr<FJsonObject>* PriceJson = nullptr;

    if (!Json->TryGetStringField(TEXT("order_id"), OutOrder.OrderId) ||
        !Json->TryGetStringField(
            TEXT("purchase_intent_id"),
            OutOrder.PurchaseIntentId) ||
        !Json->TryGetStringField(
            TEXT("product_id"),
            ProductId) ||
        !Json->TryGetStringField(
            TEXT("offer_id"),
            OfferId) ||
        !Json->TryGetNumberField(
            TEXT("quantity"),
            Quantity) ||
        !Json->TryGetStringField(
            TEXT("order_state"),
            OutOrder.OrderState) ||
        !Json->TryGetStringField(
            TEXT("payment_state"),
            OutOrder.PaymentState) ||
        !Json->TryGetStringField(
            TEXT("fulfillment_state"),
            OutOrder.FulfillmentState) ||
        !NumberToInt64(
            Json,
            TEXT("order_revision"),
            OrderRevision) ||
        !Json->TryGetObjectField(
            TEXT("price_snapshot"),
            PriceJson) ||
        !PriceJson ||
        !JsonToPrice(
            *PriceJson,
            OutOrder.Price,
            &OutOrder.TotalAmountMinor))
    {
        return false;
    }

    OutOrder.ProductId = FName(*ProductId);
    OutOrder.OfferId = FName(*OfferId);
    OutOrder.Quantity = static_cast<int32>(Quantity);
    OutOrder.OrderRevision = OrderRevision;

    const TSharedPtr<FJsonObject>* FlowJson = nullptr;
    if (Json->TryGetObjectField(TEXT("provider_flow"), FlowJson) &&
        FlowJson &&
        (*FlowJson).IsValid())
    {
        (*FlowJson)->TryGetStringField(
            TEXT("provider_name"),
            OutOrder.ProviderFlow.ProviderName);
        (*FlowJson)->TryGetStringField(
            TEXT("session_id"),
            OutOrder.ProviderFlow.SessionId);
        (*FlowJson)->TryGetStringField(
            TEXT("client_token"),
            OutOrder.ProviderFlow.ClientToken);
        (*FlowJson)->TryGetBoolField(
            TEXT("non_production"),
            OutOrder.ProviderFlow.bNonProduction);
    }

    OutOrder.bPurchaseSucceeded =
        OutOrder.OrderState == TEXT("fulfilled") &&
        OutOrder.PaymentState == TEXT("confirmed") &&
        OutOrder.FulfillmentState == TEXT("fulfilled");

    OutOrder.bPaymentConfirmedRewardPending =
        OutOrder.PaymentState == TEXT("confirmed") &&
        OutOrder.FulfillmentState != TEXT("fulfilled");

    return !OutOrder.OrderId.IsEmpty() &&
           OutOrder.Quantity > 0;
}

bool FGamePlatformCommerceGatewayHttpTransport::JsonToPrice(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformCommercePriceView& OutPrice,
    int64* OutTotalAmountMinor)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString PriceId;
    FString PriceType;
    FString CurrencyCode;
    FString CurrencyId;
    int64 UnitAmountMinor = 0;

    if (!Json->TryGetStringField(TEXT("price_id"), PriceId) ||
        !Json->TryGetStringField(TEXT("price_type"), PriceType) ||
        !NumberToInt64(
            Json,
            TEXT("unit_amount_minor"),
            UnitAmountMinor) ||
        UnitAmountMinor < 0)
    {
        return false;
    }

    Json->TryGetStringField(TEXT("currency_code"), CurrencyCode);
    Json->TryGetStringField(TEXT("currency_id"), CurrencyId);

    OutPrice.PriceId = FName(*PriceId);
    OutPrice.PriceType = ParsePriceType(PriceType);
    OutPrice.CurrencyCode = CurrencyCode;
    OutPrice.CurrencyId = CurrencyId;
    OutPrice.UnitAmountMinor = UnitAmountMinor;
    OutPrice.DisplayText = FormatDisplayPrice(OutPrice);

    if (OutPrice.PriceType == EGamePlatformCommercePriceType::Unknown)
    {
        return false;
    }

    if (OutTotalAmountMinor)
    {
        int64 Total = UnitAmountMinor;
        if (NumberToInt64(
                Json,
                TEXT("total_amount_minor"),
                Total))
        {
            *OutTotalAmountMinor = Total;
        }
        else
        {
            *OutTotalAmountMinor = UnitAmountMinor;
        }
    }

    return true;
}

EGamePlatformCommercePriceType
FGamePlatformCommerceGatewayHttpTransport::ParsePriceType(
    const FString& Value)
{
    if (Value == TEXT("real_money"))
    {
        return EGamePlatformCommercePriceType::RealMoney;
    }
    if (Value == TEXT("soft_currency"))
    {
        return EGamePlatformCommercePriceType::SoftCurrency;
    }
    if (Value == TEXT("free"))
    {
        return EGamePlatformCommercePriceType::Free;
    }
    return EGamePlatformCommercePriceType::Unknown;
}

FString FGamePlatformCommerceGatewayHttpTransport::FormatDisplayPrice(
    const FGamePlatformCommercePriceView& Price)
{
    if (Price.PriceType ==
        EGamePlatformCommercePriceType::RealMoney)
    {
        // 显示文本不是权威扣款值；权威金额始终是UnitAmountMinor。
        const int64 Whole = Price.UnitAmountMinor / 100;
        const int64 Minor = FMath::Abs(Price.UnitAmountMinor % 100);
        return FString::Printf(
            TEXT("%s %lld.%02lld"),
            *Price.CurrencyCode,
            Whole,
            Minor);
    }

    if (Price.PriceType ==
        EGamePlatformCommercePriceType::SoftCurrency)
    {
        return FString::Printf(
            TEXT("%s %lld"),
            *Price.CurrencyId,
            Price.UnitAmountMinor);
    }

    if (Price.PriceType ==
        EGamePlatformCommercePriceType::Free)
    {
        return TEXT("Free");
    }

    return FString();
}

bool FGamePlatformCommerceGatewayHttpTransport::ParseIso8601(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    FDateTime& OutValue,
    bool bRequired)
{
    FString Value;
    if (!Json.IsValid() ||
        !Json->TryGetStringField(Field, Value) ||
        Value.IsEmpty())
    {
        return !bRequired;
    }

    return FDateTime::ParseIso8601(*Value, OutValue);
}

EGamePlatformCommerceError
FGamePlatformCommerceGatewayHttpTransport::MapHttpError(
    int32 StatusCode,
    const FString& Body)
{
    const FString Lower = Body.ToLower();

    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformCommerceError::Unauthorized;
    }

    if (StatusCode == 404)
    {
        if (Lower.Contains(TEXT("order")))
        {
            return EGamePlatformCommerceError::OrderNotFound;
        }
        if (Lower.Contains(TEXT("product")))
        {
            return EGamePlatformCommerceError::ProductNotFound;
        }
        if (Lower.Contains(TEXT("offer")))
        {
            return EGamePlatformCommerceError::OfferNotFound;
        }
        return EGamePlatformCommerceError::CatalogUnavailable;
    }

    if (StatusCode == 402)
    {
        if (Lower.Contains(TEXT("verification")))
        {
            return EGamePlatformCommerceError::PaymentVerificationFailed;
        }
        return EGamePlatformCommerceError::PaymentFailed;
    }

    if (StatusCode == 409)
    {
        if (Lower.Contains(TEXT("not active")))
        {
            return EGamePlatformCommerceError::OfferNotActive;
        }
        if (Lower.Contains(TEXT("not eligible")))
        {
            return EGamePlatformCommerceError::OfferNotEligible;
        }
        if (Lower.Contains(TEXT("offer revision")))
        {
            return EGamePlatformCommerceError::OfferRevisionMismatch;
        }
        if (Lower.Contains(TEXT("price")))
        {
            return EGamePlatformCommerceError::PriceChanged;
        }
        if (Lower.Contains(TEXT("limit")))
        {
            return EGamePlatformCommerceError::PurchaseLimitReached;
        }
        if (Lower.Contains(TEXT("intent expired")))
        {
            return EGamePlatformCommerceError::PurchaseIntentExpired;
        }
        if (Lower.Contains(TEXT("receipt replay")))
        {
            return EGamePlatformCommerceError::ReceiptReplay;
        }
        if (Lower.Contains(TEXT("soft currency")))
        {
            return EGamePlatformCommerceError::SoftCurrencyUnavailable;
        }
        return EGamePlatformCommerceError::InvalidOrderState;
    }

    if (StatusCode == 0 || StatusCode >= 500)
    {
        if (Lower.Contains(TEXT("provider")))
        {
            return EGamePlatformCommerceError::ProviderUnavailable;
        }
        if (Lower.Contains(TEXT("fulfillment")))
        {
            return EGamePlatformCommerceError::FulfillmentFailed;
        }
        if (Lower.Contains(TEXT("timed out")))
        {
            return EGamePlatformCommerceError::TimedOut;
        }
        return EGamePlatformCommerceError::BackendUnavailable;
    }

    return EGamePlatformCommerceError::OutcomeUnknown;
}
