// 平台客户端业务JSON适配；线程、生命周期与迁移合同见同名公开头。
#include "Transport/GamePlatformLiveOpsGatewayHttpTransport.h"

#include "Dom/JsonObject.h"
#include "JsonIntegerPolicy.h"
#include "GamePlatformOnlineClientSubsystem.h"
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
bool NumberToInt64(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, int64& Out)
{
    if (!Json) { return false; }
    const auto* Value = Json->Values.Find(Field);
    if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Number) { return false; }
    FString Token;
    if ((*Value)->PreferStringRepresentation() && (*Value)->TryGetString(Token))
    {
        std::int64_t Exact = 0;
        if (!GPIntegerPolicy::ParseJsonInteger(std::basic_string_view<TCHAR>(*Token, Token.Len()), Exact)) { return false; }
        Out = Exact; return true;
    }
    double Number = 0;
    if (!(*Value)->TryGetNumber(Number) || !FMath::IsFinite(Number) || FMath::FloorToDouble(Number) != Number || Number < -9007199254740991.0 || Number > 9007199254740991.0) { return false; }
    Out = static_cast<int64>(Number); return true;
}
bool NumberToInt32(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, int32& Out)
{
    int64 Exact = 0;
    if (!NumberToInt64(Json, Field, Exact) || Exact < MIN_int32 || Exact > MAX_int32) { return false; }
    Out = static_cast<int32>(Exact); return true;
}
// 缺省可选集合沿用当前合同；明确提供的null/对象/字符串不能冒充成功空数组。
bool OptionalArrayHasValidType(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
{ return Json && (!Json->HasField(Field) || Json->HasTypedField<EJson::Array>(Field)); }

}

// 游戏线程中的本领域所有权账本；不保存Token，也不拥有Online/HTTP对象。
struct FGamePlatformLiveOpsGatewayHttpTransport::FRuntime
{
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TArray<FGamePlatformOnlineRequestHandle> ActiveRequests;
    uint64 CancellationGeneration = 0;
};

FGamePlatformLiveOpsGatewayHttpTransport::FGamePlatformLiveOpsGatewayHttpTransport(UGamePlatformOnlineClientSubsystem* InOnlineSubsystem)
    : Runtime(MakeUnique<FRuntime>())
{
    check(IsInGameThread());
    Runtime->Online = InOnlineSubsystem;
}

FGamePlatformLiveOpsGatewayHttpTransport::FGamePlatformLiveOpsGatewayHttpTransport(FString InGatewayBaseUrl, FString InAccessToken)
    : Runtime(MakeUnique<FRuntime>())
{
    // 旧消费者须迁入Online组合根；不把传入票据复制到长期对象或日志。
    (void)InGatewayBaseUrl;
    InAccessToken.Reset();
}

FGamePlatformLiveOpsGatewayHttpTransport::~FGamePlatformLiveOpsGatewayHttpTransport()
{
    CancelAllRequests();
}

bool FGamePlatformLiveOpsGatewayHttpTransport::IsConfigured() const
{
    check(IsInGameThread());
    const auto* Online = Runtime ? Runtime->Online.Get() : nullptr;
    if (!IsValid(Online)) { return false; }
    const auto State = Online->GetSnapshot().State;
    return State == EGamePlatformAuthState::Authenticated || State == EGamePlatformAuthState::Refreshing;
}

void FGamePlatformLiveOpsGatewayHttpTransport::CancelAllRequests()
{
    check(IsInGameThread());
    if (!Runtime) { return; }
    ++Runtime->CancellationGeneration;
    auto Requests = MoveTemp(Runtime->ActiveRequests);
    Runtime->ActiveRequests.Reset();
    if (auto* Online = Runtime->Online.Get())
    {
        for (const auto& Request : Requests) { Online->Cancel(Request); }
    }
}

void FGamePlatformLiveOpsGatewayHttpTransport::UnregisterRequest(const FGuid& RequestId)
{
    Runtime->ActiveRequests.RemoveAll([&RequestId](const auto& Handle) { return Handle.RequestId == RequestId; });
}

bool FGamePlatformLiveOpsGatewayHttpTransport::StartJsonRequest(
    const FString& Verb,
    const FString& Path,
    const TSharedPtr<FJsonObject>& Body,
    TFunction<void(int32, const FString&, EGamePlatformLiveOpsError)> Completion)
{
    check(IsInGameThread());
    if (!Completion || !IsConfigured() || Runtime->ActiveRequests.Num() >= 8) { return false; }
    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = Verb;
    Request.RelativePath = Path;
    // 只读请求允许Online重试；写命令没有端点级幂等合同前禁止自动重放。
    Request.bIdempotent = Request.Verb == TEXT("GET") || Request.Verb == TEXT("HEAD");
    if (Body.IsValid())
    {
        const auto Writer = TJsonWriterFactory<>::Create(&Request.Body);
        if (!FJsonSerializer::Serialize(Body.ToSharedRef(), Writer)) { return false; }
    }
    const uint64 ExpectedCancellationGeneration = Runtime->CancellationGeneration;
    const TWeakPtr<FGamePlatformLiveOpsGatewayHttpTransport, ESPMode::ThreadSafe> WeakSelf = AsShared();
    const auto HandleBox = MakeShared<FGamePlatformOnlineRequestHandle, ESPMode::ThreadSafe>();
    auto Handle = Runtime->Online->SendAuthenticatedRequest(
        MoveTemp(Request), FGamePlatformOnlineRequestOptions(),
        [WeakSelf, HandleBox, ExpectedCancellationGeneration, Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            const auto Self = WeakSelf.Pin();
            if (!Self || !Self->Runtime || Self->Runtime->CancellationGeneration != ExpectedCancellationGeneration) { return; }
            Self->UnregisterRequest(HandleBox->RequestId);
            EGamePlatformLiveOpsError Error = EGamePlatformLiveOpsError::None;
            if (!Response.IsSuccess())
            {
                if (Response.Error == EGamePlatformAuthError::AuthExpired ||
                    Response.Error == EGamePlatformAuthError::InvalidCredentials ||
                    Response.Error == EGamePlatformAuthError::Forbidden)
                { Error = EGamePlatformLiveOpsError::Unauthorized; }
                else if (Response.Error == EGamePlatformAuthError::Cancelled)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformLiveOpsError::OutcomeUnknown : EGamePlatformLiveOpsError::Cancelled; }
                else if (Response.Error == EGamePlatformAuthError::TimedOut)
                { Error = Response.bMayHaveReachedServer ? EGamePlatformLiveOpsError::OutcomeUnknown : EGamePlatformLiveOpsError::TimedOut; }
                else if (Response.Error == EGamePlatformAuthError::OutcomeUnknown)
                { Error = EGamePlatformLiveOpsError::OutcomeUnknown; }
                else { Error = MapHttpError(Response.HttpStatusCode, Response.Body); }
            }
            Completion(Response.HttpStatusCode, Response.Body, Error);
        });
    *HandleBox = Handle;
    if (!Handle.RequestId.IsValid()) { return false; }
    Runtime->ActiveRequests.Add(Handle);
    return true;
}

bool FGamePlatformLiveOpsGatewayHttpTransport::BeginGetCatalog(
    FGamePlatformLiveOpsCatalogCompletion Completion)
{
    if (!Completion) { return false; }
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/liveops/catalog"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body,
            EGamePlatformLiveOpsError TransportError) mutable
        {
            if (TransportError != EGamePlatformLiveOpsError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    TransportError != EGamePlatformLiveOpsError::None ? TransportError : MapHttpError(StatusCode, Body));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformLiveOpsCatalogSnapshot Catalog;
            if (!FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings) ||
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
    if (!Completion) { return false; }
    return StartJsonRequest(
        TEXT("GET"),
        TEXT("/v1/liveops/state"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            int32 StatusCode,
            const FString& Body,
            EGamePlatformLiveOpsError TransportError) mutable
        {
            if (TransportError != EGamePlatformLiveOpsError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    TransportError != EGamePlatformLiveOpsError::None ? TransportError : MapHttpError(StatusCode, Body));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Body);

            FGamePlatformLiveOpsPlayerState State;
            if (!FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings) ||
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
    if (!Completion) { return false; }
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
            const FString& ResponseBody,
            EGamePlatformLiveOpsError TransportError) mutable
        {
            if (TransportError != EGamePlatformLiveOpsError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    TransportError != EGamePlatformLiveOpsError::None ? TransportError : MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformLiveOpsClaimResult Claim;
            if (!FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings) ||
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
    if (!Completion) { return false; }
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
            const FString& ResponseBody,
            EGamePlatformLiveOpsError TransportError) mutable
        {
            if (TransportError != EGamePlatformLiveOpsError::None || StatusCode < 200 || StatusCode >= 300)
            {
                Completion(
                    {},
                    TransportError != EGamePlatformLiveOpsError::None ? TransportError : MapHttpError(StatusCode, ResponseBody));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseBody);

            FGamePlatformLiveOpsClaimResult Claim;
            if (!FJsonSerializer::Deserialize(Reader, Json, FJsonSerializer::EFlags::StoreNumbersAsStrings) ||
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

    int32 Version = 0;
    int64 Revision = 0;

    if (!NumberToInt32((*CatalogJson), TEXT("catalog_version"), Version) ||
        !NumberToInt64((*CatalogJson), TEXT("catalog_revision"), Revision) ||
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

    if (!OptionalArrayHasValidType(*CatalogJson, TEXT("seasons"))) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Seasons = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("seasons"),
            Seasons) &&
        Seasons)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Seasons)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString NameKey;
            FString Presentation;
            int32 ItemVersion = 0;
            int32 Priority = 0;

            FGamePlatformLiveOpsSeason Season;
            if (!Object->TryGetStringField(TEXT("season_id"), Id) ||
                !Object->TryGetStringField(TEXT("name_key"), NameKey) ||
                !NumberToInt32(Object, TEXT("version"), ItemVersion) ||
                !NumberToInt32(Object, TEXT("priority"), Priority) ||
                !ParseTimeWindow(Object, Season.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            if (Id.IsEmpty() || ItemVersion < 1) { return false; }
            Season.SeasonId = FName(*Id);
            Season.Version = static_cast<int32>(ItemVersion);
            Season.NameKey = NameKey;
            Season.PresentationMetadataId = FName(*Presentation);
            Season.Priority = static_cast<int32>(Priority);
            OutCatalog.Seasons.Add(MoveTemp(Season));
        }
    }

    if (!OptionalArrayHasValidType(*CatalogJson, TEXT("events"))) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("events"),
            Events) &&
        Events)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Events)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString Type;
            FString SeasonId;
            FString Presentation;
            int32 Priority = 0;

            FGamePlatformLiveOpsEvent Event;
            if (!Object->TryGetStringField(TEXT("event_id"), Id) ||
                !Object->TryGetStringField(TEXT("event_type"), Type) ||
                !NumberToInt32(Object, TEXT("priority"), Priority) ||
                !ParseTimeWindow(Object, Event.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(TEXT("season_id"), SeasonId);
            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            if (Id.IsEmpty()) { return false; }
            Event.EventId = FName(*Id);
            Event.EventType = FName(*Type);
            Event.SeasonId = FName(*SeasonId);
            Event.PresentationMetadataId = FName(*Presentation);
            Event.Priority = static_cast<int32>(Priority);

            if (!OptionalArrayHasValidType(Object, TEXT("tags"))) { return false; }
            const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
            if (Object->TryGetArrayField(TEXT("tags"), Tags) && Tags)
            {
                for (const TSharedPtr<FJsonValue>& Tag : *Tags)
                {
                    FString TagString;
                    if (Tag.IsValid() && Tag->Type == EJson::String &&
                        Tag->TryGetString(TagString) &&
                        !TagString.IsEmpty())
                    {
                        Event.Tags.Add(FName(*TagString));
                    }
                    else { return false; }
                }
            }

            OutCatalog.Events.Add(MoveTemp(Event));
        }
    }

    if (!OptionalArrayHasValidType(*CatalogJson, TEXT("sign_in_campaigns"))) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Campaigns = nullptr;
    if ((*CatalogJson)->TryGetArrayField(
            TEXT("sign_in_campaigns"),
            Campaigns) &&
        Campaigns)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Campaigns)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString Presentation;
            int32 ItemVersion = 0;

            FGamePlatformLiveOpsSignInCampaign Campaign;
            if (!Object->TryGetStringField(TEXT("campaign_id"), Id) ||
                !NumberToInt32(Object, TEXT("version"), ItemVersion) ||
                !ParseTimeWindow(Object, Campaign.TimeWindow))
            {
                return false;
            }

            Object->TryGetStringField(
                TEXT("presentation_metadata_id"),
                Presentation);

            if (Id.IsEmpty() || ItemVersion < 1) { return false; }
            Campaign.CampaignId = FName(*Id);
            Campaign.Version = static_cast<int32>(ItemVersion);
            Campaign.PresentationMetadataId = FName(*Presentation);

            if (!OptionalArrayHasValidType(Object, TEXT("reward_schedule"))) { return false; }
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

    int64 Revision = 0;
    if (!NumberToInt64(Json, TEXT("player_state_revision"), Revision) ||
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

    if (!OptionalArrayHasValidType(Json, TEXT("campaign_states"))) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Campaigns = nullptr;
    if (Json->TryGetArrayField(
            TEXT("campaign_states"),
            Campaigns) &&
        Campaigns)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Campaigns)
        {
            const TSharedPtr<FJsonObject> Object =
                Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                return false;
            }

            FString Id;
            FString PeriodKey;
            FString RewardStatus;
            bool bClaimed = false;
            int32 TotalClaims = 0;
            int32 NextReward = 0;

            if (!Object->TryGetStringField(TEXT("campaign_id"), Id) ||
                !Object->TryGetStringField(
                    TEXT("current_period_key"),
                    PeriodKey) ||
                !Object->TryGetBoolField(
                    TEXT("claimed_current_period"),
                    bClaimed) ||
                !NumberToInt32(Object, TEXT("total_claim_count"), TotalClaims) ||
                !NumberToInt32(Object, TEXT("next_reward_index"), NextReward) ||
                !Object->TryGetStringField(
                    TEXT("reward_status"),
                    RewardStatus))
            {
                return false;
            }

            FGamePlatformLiveOpsCampaignState Campaign;
            if (Id.IsEmpty() || TotalClaims < 0 || NextReward < 0) { return false; }
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
    int64 Revision = 0;

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
        !NumberToInt64(Json, TEXT("player_state_revision"), Revision) ||
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

    return Revision > 0 && !OutClaim.ClaimId.IsEmpty() &&
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
