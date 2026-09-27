#include "Transport/GamePlatformLiveOpsGatewayHttpTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString GuidString(const FGuid& Guid)
{
    return Guid.IsValid()
        ? Guid.ToString(EGuidFormats::DigitsWithHyphensLower)
        : FString();
}
}

FGamePlatformLiveOpsGatewayHttpTransport::
FGamePlatformLiveOpsGatewayHttpTransport(
    FString InGatewayBaseUrl,
    FString InAccessToken)
    : GatewayBaseUrl(MoveTemp(InGatewayBaseUrl))
    , AccessToken(MoveTemp(InAccessToken))
{
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool FGamePlatformLiveOpsGatewayHttpTransport::IsConfigured() const
{
    return !GatewayBaseUrl.IsEmpty() &&
           !AccessToken.IsEmpty();
}

void FGamePlatformLiveOpsGatewayHttpTransport::CancelAllRequests()
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

bool FGamePlatformLiveOpsGatewayHttpTransport::StartJsonRequest(
    const FString& Verb,
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    TFunction<void(int32, const FString&)> Completion)
{
    if (!IsConfigured() ||
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
        FGamePlatformLiveOpsGatewayHttpTransport,
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

void FGamePlatformLiveOpsGatewayHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&ActiveRequestsMutex);
    ActiveRequests.Remove(Request);
}

bool FGamePlatformLiveOpsGatewayHttpTransport::BeginGetCatalog(
    FGamePlatformLiveOpsCatalogCompletion Completion)
{
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/liveops/catalog"),
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

            FGamePlatformLiveOpsCatalogSnapshot Catalog;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToCatalog(Json, Catalog))
            {
                Completion(
                    {},
                    EGamePlatformLiveOpsError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Catalog),
                EGamePlatformLiveOpsError::None);
        });
}

bool FGamePlatformLiveOpsGatewayHttpTransport::BeginGetPlayerState(
    FGamePlatformLiveOpsPlayerStateCompletion Completion)
{
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/liveops/state"),
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

            FGamePlatformLiveOpsPlayerState State;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToPlayerState(Json, State))
            {
                Completion(
                    {},
                    EGamePlatformLiveOpsError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(State),
                EGamePlatformLiveOpsError::None);
        });
}

bool FGamePlatformLiveOpsGatewayHttpTransport::BeginClaimSignIn(
    const FGuid& ClaimOperationId,
    FName CampaignId,
    FGamePlatformLiveOpsClaimCompletion Completion)
{
    if (!ClaimOperationId.IsValid() ||
        CampaignId.IsNone())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Body =
        MakeShared<FJsonObject>();

    Body->SetStringField(
        TEXT("claim_operation_id"),
        GuidString(ClaimOperationId));
    Body->SetStringField(
        TEXT("campaign_id"),
        CampaignId.ToString());

    return StartJsonRequest(
        TEXT("POST"),
        TEXT("/v1/liveops/signin/claim"),
        Body,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(
                        StatusCode,
                        ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformLiveOpsClaimResult Claim;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToClaim(Json, Claim))
            {
                Completion(
                    {},
                    EGamePlatformLiveOpsError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Claim),
                EGamePlatformLiveOpsError::None);
        });
}

bool FGamePlatformLiveOpsGatewayHttpTransport::BeginQueryClaimOperation(
    const FGuid& ClaimOperationId,
    FGamePlatformLiveOpsClaimCompletion Completion)
{
    if (!ClaimOperationId.IsValid())
    {
        return false;
    }

    return StartJsonRequest(
        TEXT("GET"),
        FString::Printf(
            TEXT("/v1/liveops/claims/%s"),
            *GuidString(ClaimOperationId)),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& ResponseBody) mutable
        {
            if (StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    MapHttpError(
                        StatusCode,
                        ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformLiveOpsClaimResult Claim;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToClaim(Json, Claim))
            {
                Completion(
                    {},
                    EGamePlatformLiveOpsError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Claim),
                EGamePlatformLiveOpsError::None);
        });
}

bool FGamePlatformLiveOpsGatewayHttpTransport::JsonToCatalog(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformLiveOpsCatalogSnapshot& OutCatalog)
{
    if (!Json.IsValid())
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* CatalogJson = nullptr;
    if (!Json->TryGetObjectField(
            TEXT("catalog"),
            CatalogJson) ||
        !CatalogJson ||
        !(*CatalogJson).IsValid() ||
        !ParseIsoTime(
            Json,
            TEXT("server_time_utc"),
            OutCatalog.ServerTimeUtc,
            true))
    {
        return false;
    }

    double Version = 0.0;
    double Revision = 0.0;

    if (!(*CatalogJson)->TryGetNumberField(
            TEXT("catalog_version"),
            Version) ||
        !(*CatalogJson)->TryGetNumberField(
            TEXT("catalog_revision"),
            Revision) ||
        Version < 1.0 ||
        Revision < 1.0)
    {
        return false;
    }

    OutCatalog.CatalogVersion =
        static_cast<int32>(Version);
    OutCatalog.CatalogRevision =
        static_cast<int64>(Revision);

    ParseIsoTime(
        *CatalogJson,
        TEXT("published_at"),
        OutCatalog.PublishedAtUtc,
        false);

    const TArray<TSharedPtr<FJsonValue>>* Seasons = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("seasons"),
            Seasons) &&
        Seasons)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Seasons)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString NameKey;
            FString Presentation;
            double ItemVersion = 0.0;
            double Priority = 0.0;

            FGamePlatformLiveOpsSeason Season;
            if (!Object->TryGetStringField(TEXT("season_id"), Id) ||
                !Object->TryGetStringField(TEXT("name_key"), NameKey) ||
                !Object->TryGetNumberField(TEXT("version"), ItemVersion) ||
                !Object->TryGetNumberField(TEXT("priority"), Priority) ||
                !ParseTimeWindow(Object, Season.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            Season.SeasonId = FName(*Id);
            Season.Version = static_cast<int32>(ItemVersion);
            Season.NameKey = NameKey;
            Season.PresentationMetadataId = FName(*Presentation);
            Season.Priority = static_cast<int32>(Priority);
            OutCatalog.Seasons.Add(MoveTemp(Season));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("events"),
            Events) &&
        Events)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Events)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString Type;
            FString SeasonId;
            FString Presentation;
            double Priority = 0.0;

            FGamePlatformLiveOpsEvent Event;
            if (!Object->TryGetStringField(TEXT("event_id"), Id) ||
                !Object->TryGetStringField(TEXT("event_type"), Type) ||
                !Object->TryGetNumberField(TEXT("priority"), Priority) ||
                !ParseTimeWindow(Object, Event.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(TEXT("season_id"), SeasonId);
            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            Event.EventId = FName(*Id);
            Event.EventType = FName(*Type);
            Event.SeasonId = FName(*SeasonId);
            Event.PresentationMetadataId = FName(*Presentation);
            Event.Priority = static_cast<int32>(Priority);

            const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
            if (Object->TryGetArrayField(TEXT("tags"), Tags) && Tags)
            {
                for (const TSharedPtr<FJsonValue>& Tag : *Tags)
                {
                    FString TagString;
                    if (Tag.IsValid() &&
                        Tag->TryGetString(TagString) &&
                        !TagString.IsEmpty())
                    {
                        Event.Tags.Add(FName(*TagString));
                    }
                }
            }

            OutCatalog.Events.Add(MoveTemp(Event));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Campaigns = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("sign_in_campaigns"),
            Campaigns) &&
        Campaigns)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Campaigns)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString Presentation;
            double ItemVersion = 0.0;

            FGamePlatformLiveOpsSignInCampaign Campaign;
            if (!Object->TryGetStringField(TEXT("campaign_id"), Id) ||
                !Object->TryGetNumberField(TEXT("version"), ItemVersion) ||
                !ParseTimeWindow(Object, Campaign.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            Campaign.CampaignId = FName(*Id);
            Campaign.Version = static_cast<int32>(ItemVersion);
            Campaign.PresentationMetadataId = FName(*Presentation);

            const TArray<TSharedPtr<FJsonValue>>* Schedule = nullptr;
            if (Object->TryGetArrayField(
                    TEXT("reward_schedule"),
                    Schedule) &&
                Schedule)
            {
                Campaign.RewardCount = Schedule->Num();
            }

            OutCatalog.SignInCampaigns.Add(MoveTemp(Campaign));
        }
    }

    return true;
}

bool FGamePlatformLiveOpsGatewayHttpTransport::JsonToPlayerState(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformLiveOpsPlayerState& OutState)
{
    if (!Json.IsValid())
    {
        return false;
    }

    double Revision = 0.0;
    if (!Json->TryGetNumberField(
            TEXT("player_state_revision"),
            Revision) ||
        Revision < 1.0 ||
        !ParseIsoTime(
            Json,
            TEXT("generated_at"),
            OutState.GeneratedAtUtc,
            true) ||
        !ParseIsoTime(
            Json,
            TEXT("server_time_utc"),
            OutState.ServerTimeUtc,
            true))
    {
        return false;
    }

    OutState.PlayerStateRevision =
        static_cast<int64>(Revision);

    const TArray<TSharedPtr<FJsonValue>>* Campaigns = nullptr;
    if (Json->TryGetArrayField(
            TEXT("campaign_states"),
            Campaigns) &&
        Campaigns)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Campaigns)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString PeriodKey;
            FString RewardStatus;
            bool bClaimed = false;
            double TotalClaims = 0.0;
            double NextReward = 0.0;

            if (!Object->TryGetStringField(TEXT("campaign_id"), Id) ||
                !Object->TryGetStringField(
                    TEXT("current_period_key"),
                    PeriodKey) ||
                !Object->TryGetBoolField(
                    TEXT("claimed_current_period"),
                    bClaimed) ||
                !Object->TryGetNumberField(
                    TEXT("total_claim_count"),
                    TotalClaims) ||
                !Object->TryGetNumberField(
                    TEXT("next_reward_index"),
                    NextReward) ||
                !Object->TryGetStringField(
                    TEXT("reward_status"),
                    RewardStatus))
            {
                return false;
            }

            FGamePlatformLiveOpsCampaignState Campaign;
            Campaign.CampaignId = FName(*Id);
            Campaign.CurrentPeriodKey = PeriodKey;
            Campaign.bClaimedCurrentPeriod = bClaimed;
            Campaign.TotalClaimCount =
                static_cast<int32>(TotalClaims);
            Campaign.NextRewardIndex =
                static_cast<int32>(NextReward);
            Campaign.RewardStatus = RewardStatus;
            OutState.CampaignStates.Add(MoveTemp(Campaign));
        }
    }

    return true;
}

bool FGamePlatformLiveOpsGatewayHttpTransport::JsonToClaim(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformLiveOpsClaimResult& OutClaim)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString CampaignId;
    double Revision = 0.0;

    if (!Json->TryGetStringField(TEXT("claim_id"), OutClaim.ClaimId) ||
        !Json->TryGetStringField(
            TEXT("claim_operation_id"),
            OutClaim.ClaimOperationId) ||
        !Json->TryGetStringField(
            TEXT("campaign_id"),
            CampaignId) ||
        !Json->TryGetStringField(
            TEXT("period_key"),
            OutClaim.PeriodKey) ||
        !Json->TryGetStringField(
            TEXT("status"),
            OutClaim.Status) ||
        !Json->TryGetNumberField(
            TEXT("player_state_revision"),
            Revision) ||
        !ParseIsoTime(
            Json,
            TEXT("server_time_utc"),
            OutClaim.ServerTimeUtc,
            true))
    {
        return false;
    }

    OutClaim.CampaignId = FName(*CampaignId);
    OutClaim.PlayerStateRevision =
        static_cast<int64>(Revision);

    return !OutClaim.ClaimId.IsEmpty() &&
           !OutClaim.ClaimOperationId.IsEmpty() &&
           !OutClaim.CampaignId.IsNone();
}

bool FGamePlatformLiveOpsGatewayHttpTransport::ParseTimeWindow(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformLiveOpsTimeWindow& OutWindow)
{
    if (!ParseIsoTime(
            Json,
            TEXT("starts_at_utc"),
            OutWindow.StartsAtUtc,
            true))
    {
        return false;
    }

    FString End;
    if (Json->TryGetStringField(TEXT("ends_at_utc"), End) &&
        !End.IsEmpty())
    {
        if (!FDateTime::ParseIso8601(
                *End,
                OutWindow.EndsAtUtc))
        {
            return false;
        }
        OutWindow.bHasEnd = true;
    }

    return !OutWindow.bHasEnd ||
           OutWindow.EndsAtUtc > OutWindow.StartsAtUtc;
}

bool FGamePlatformLiveOpsGatewayHttpTransport::ParseIsoTime(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    FDateTime& OutTime,
    bool bRequired)
{
    FString Value;
    if (!Json.IsValid() ||
        !Json->TryGetStringField(Field, Value) ||
        Value.IsEmpty())
    {
        return !bRequired;
    }

    return FDateTime::ParseIso8601(*Value, OutTime);
}

EGamePlatformLiveOpsError
FGamePlatformLiveOpsGatewayHttpTransport::MapHttpError(
    int32 StatusCode,
    const FString& Body)
{
    const FString Lower = Body.ToLower();

    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformLiveOpsError::Unauthorized;
    }

    if (StatusCode == 404)
    {
        if (Lower.Contains(TEXT("campaign")))
        {
            return EGamePlatformLiveOpsError::CampaignNotFound;
        }
        return EGamePlatformLiveOpsError::CatalogUnavailable;
    }

    if (StatusCode == 409)
    {
        if (Lower.Contains(TEXT("already claimed")))
        {
            return EGamePlatformLiveOpsError::AlreadyClaimed;
        }
        if (Lower.Contains(TEXT("in progress")))
        {
            return EGamePlatformLiveOpsError::ClaimInProgress;
        }
        if (Lower.Contains(TEXT("not active")))
        {
            return EGamePlatformLiveOpsError::CampaignNotActive;
        }
        if (Lower.Contains(TEXT("not eligible")))
        {
            return EGamePlatformLiveOpsError::NotEligible;
        }
        if (Lower.Contains(TEXT("preflight")))
        {
            return EGamePlatformLiveOpsError::RewardPreflightFailed;
        }
        return EGamePlatformLiveOpsError::OutcomeUnknown;
    }

    if (StatusCode == 0 || StatusCode >= 500)
    {
        if (Lower.Contains(TEXT("reward")))
        {
            return EGamePlatformLiveOpsError::RewardGrantFailed;
        }
        return EGamePlatformLiveOpsError::BackendUnavailable;
    }

    return EGamePlatformLiveOpsError::OutcomeUnknown;
}
