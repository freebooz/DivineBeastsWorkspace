#include "Transport/GamePlatformEntitlementGatewayHttpTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FGamePlatformEntitlementGatewayHttpTransport::
FGamePlatformEntitlementGatewayHttpTransport(
    FString InGatewayBaseUrl,
    FString InAccessToken)
    : GatewayBaseUrl(MoveTemp(InGatewayBaseUrl))
    , AccessToken(MoveTemp(InAccessToken))
{
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool FGamePlatformEntitlementGatewayHttpTransport::IsConfigured() const
{
    return !GatewayBaseUrl.IsEmpty() && !AccessToken.IsEmpty();
}

void FGamePlatformEntitlementGatewayHttpTransport::CancelAllRequests()
{
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> Requests;

    {
        FScopeLock Lock(&ActiveRequestsMutex);
        Requests = ActiveRequests;
        ActiveRequests.Reset();
    }

    for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request : Requests)
    {
        if (Request.IsValid())
        {
            Request->CancelRequest();
        }
    }
}

void FGamePlatformEntitlementGatewayHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&ActiveRequestsMutex);
    ActiveRequests.Remove(Request);
}

bool FGamePlatformEntitlementGatewayHttpTransport::StartJsonRequest(
    const FString& Path,
    TFunction<void(int32, const FString&)> Completion)
{
    if (!IsConfigured() || !Completion)
    {
        return false;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> RequestPtr = Request;

    {
        FScopeLock Lock(&ActiveRequestsMutex);
        constexpr int32 MaxConcurrentHttpRequests = 8;
        if (ActiveRequests.Num() >= MaxConcurrentHttpRequests)
        {
            return false;
        }
        ActiveRequests.Add(RequestPtr);
    }

    TSharedRef<FGamePlatformEntitlementGatewayHttpTransport, ESPMode::ThreadSafe>
        Self = AsShared();

    Request->SetURL(GatewayBaseUrl + Path);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(
        TEXT("Authorization"),
        FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

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
            const FString Body =
                Response.IsValid()
                    ? Response->GetContentAsString()
                    : FString();

            Self->UnregisterRequest(RequestPtr);

            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Completion),
                 StatusCode,
                 Body]() mutable
                {
                    Completion(StatusCode, Body);
                });
        });

    const bool bStarted = Request->ProcessRequest();
    if (!bStarted)
    {
        UnregisterRequest(RequestPtr);
    }
    return bStarted;
}

bool FGamePlatformEntitlementGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformEntitlementSnapshotCompletion Completion)
{
    return StartJsonRequest(
        TEXT("/v1/entitlements"),
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion({}, MapHttpError(StatusCode));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformEntitlementSnapshot Snapshot;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToSnapshot(Json, Snapshot))
            {
                Completion(
                    {},
                    EGamePlatformEntitlementError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Snapshot),
                EGamePlatformEntitlementError::None);
        });
}

bool FGamePlatformEntitlementGatewayHttpTransport::JsonToSnapshot(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformEntitlementSnapshot& OutSnapshot)
{
    if (!Json.IsValid())
    {
        return false;
    }

    double Revision = 0.0;
    FString GeneratedAt;
    if (!Json->TryGetNumberField(TEXT("revision"), Revision) ||
        Revision <= 0.0 ||
        !Json->TryGetStringField(TEXT("generated_at"), GeneratedAt) ||
        !FDateTime::ParseIso8601(*GeneratedAt, OutSnapshot.GeneratedAtUtc))
    {
        return false;
    }

    OutSnapshot.Revision = static_cast<int64>(Revision);

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Json->TryGetArrayField(TEXT("entitlements"), Values) || !Values)
    {
        return true;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Object =
            Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            return false;
        }

        FString EntitlementId;
        FString Category;
        FString TargetType;
        FString TargetId;
        FString Status;

        if (!Object->TryGetStringField(TEXT("entitlement_id"), EntitlementId) ||
            !Object->TryGetStringField(TEXT("category"), Category) ||
            !Object->TryGetStringField(TEXT("target_type"), TargetType) ||
            !Object->TryGetStringField(TEXT("target_id"), TargetId) ||
            !Object->TryGetStringField(TEXT("status"), Status))
        {
            return false;
        }

        FGamePlatformEntitlementEntry Entry;
        Entry.EntitlementId = FName(*EntitlementId);
        Entry.Category = FName(*Category);
        Entry.TargetId = FName(*TargetId);

        if (TargetType == TEXT("hero"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Hero;
        else if (TargetType == TEXT("skin"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Skin;
        else if (TargetType == TEXT("cosmetic"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Cosmetic;
        else if (TargetType == TEXT("feature"))
            Entry.TargetType = EGamePlatformEntitlementTargetType::Feature;
        else
            Entry.TargetType = EGamePlatformEntitlementTargetType::Unknown;

        if (Status == TEXT("active"))
            Entry.Status = EGamePlatformEntitlementStatus::Active;
        else if (Status == TEXT("not_started"))
            Entry.Status = EGamePlatformEntitlementStatus::NotStarted;
        else if (Status == TEXT("expired"))
            Entry.Status = EGamePlatformEntitlementStatus::Expired;
        else
            Entry.Status = EGamePlatformEntitlementStatus::Revoked;

        FString Optional;
        if (Object->TryGetStringField(TEXT("starts_at"), Optional) &&
            !Optional.IsEmpty())
        {
            Entry.bHasStartsAt =
                FDateTime::ParseIso8601(*Optional, Entry.StartsAtUtc);
        }

        if (Object->TryGetStringField(TEXT("expires_at"), Optional) &&
            !Optional.IsEmpty())
        {
            Entry.bHasExpiresAt =
                FDateTime::ParseIso8601(*Optional, Entry.ExpiresAtUtc);
        }

        if (Entry.EntitlementId.IsNone() ||
            Entry.TargetType == EGamePlatformEntitlementTargetType::Unknown ||
            Entry.TargetId.IsNone())
        {
            return false;
        }

        OutSnapshot.Entitlements.Add(MoveTemp(Entry));
    }

    return true;
}

EGamePlatformEntitlementError
FGamePlatformEntitlementGatewayHttpTransport::MapHttpError(
    int32 StatusCode)
{
    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformEntitlementError::Unauthorized;
    }

    if (StatusCode == 0 || StatusCode >= 500)
    {
        return EGamePlatformEntitlementError::BackendUnavailable;
    }

    return EGamePlatformEntitlementError::OutcomeUnknown;
}
