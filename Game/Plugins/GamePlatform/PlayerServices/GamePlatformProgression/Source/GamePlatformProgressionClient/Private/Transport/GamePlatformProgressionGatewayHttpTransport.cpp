#include "Transport/GamePlatformProgressionGatewayHttpTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FGamePlatformProgressionGatewayHttpTransport::
FGamePlatformProgressionGatewayHttpTransport(
    FString InGatewayBaseUrl,
    FString InAccessToken)
    : GatewayBaseUrl(MoveTemp(InGatewayBaseUrl))
    , AccessToken(MoveTemp(InAccessToken))
{
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

bool FGamePlatformProgressionGatewayHttpTransport::IsConfigured() const
{
    return !GatewayBaseUrl.IsEmpty() &&
           !AccessToken.IsEmpty();
}

void FGamePlatformProgressionGatewayHttpTransport::CancelAllRequests()
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

bool FGamePlatformProgressionGatewayHttpTransport::StartRequest(
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

    TSharedRef<
        FGamePlatformProgressionGatewayHttpTransport,
        ESPMode::ThreadSafe> Self = AsShared();

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

void FGamePlatformProgressionGatewayHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&ActiveRequestsMutex);
    ActiveRequests.Remove(Request);
}

bool FGamePlatformProgressionGatewayHttpTransport::BeginGetSnapshot(
    FGamePlatformProgressionSnapshotCompletion Completion)
{
    return StartRequest(
        TEXT("/v1/progression"),
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

            FGamePlatformProgressionSnapshot Snapshot;
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !JsonToSnapshot(Json, Snapshot))
            {
                Completion(
                    {},
                    EGamePlatformProgressionError::InvalidResponse);
                return;
            }

            Completion(
                MoveTemp(Snapshot),
                EGamePlatformProgressionError::None);
        });
}

bool FGamePlatformProgressionGatewayHttpTransport::JsonToSnapshot(
    const TSharedPtr<FJsonObject>& Json,
    FGamePlatformProgressionSnapshot& OutSnapshot)
{
    if (!Json.IsValid())
    {
        return false;
    }

    FString GeneratedAt;
    if (!ParseInt64String(
            Json,
            TEXT("progression_revision"),
            OutSnapshot.ProgressionRevision) ||
        OutSnapshot.ProgressionRevision < 1 ||
        !Json->TryGetStringField(
            TEXT("generated_at"),
            GeneratedAt) ||
        !FDateTime::ParseIso8601(
            *GeneratedAt,
            OutSnapshot.GeneratedAtUtc))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Json->TryGetArrayField(TEXT("tracks"), Values) || !Values)
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

        FString SubjectType;
        FString SubjectId;
        FString TrackId;
        double Level = 0.0;
        double MaxLevel = 0.0;
        double CurveVersion = 0.0;

        FGamePlatformProgressionTrackState Track;

        if (!Object->TryGetStringField(
                TEXT("subject_type"),
                SubjectType) ||
            !Object->TryGetStringField(
                TEXT("subject_id"),
                SubjectId) ||
            !Object->TryGetStringField(
                TEXT("track_id"),
                TrackId) ||
            !Object->TryGetNumberField(
                TEXT("level"),
                Level) ||
            !Object->TryGetNumberField(
                TEXT("max_level"),
                MaxLevel) ||
            !Object->TryGetNumberField(
                TEXT("curve_version"),
                CurveVersion) ||
            !ParseInt64String(
                Object,
                TEXT("total_xp"),
                Track.TotalXP) ||
            !ParseInt64String(
                Object,
                TEXT("revision"),
                Track.Revision))
        {
            return false;
        }

        Track.SubjectType =
            SubjectType == TEXT("character")
                ? EGamePlatformProgressionSubjectType::Character
                : SubjectType == TEXT("player")
                    ? EGamePlatformProgressionSubjectType::Player
                    : EGamePlatformProgressionSubjectType::Unknown;

        Track.SubjectId = SubjectId;
        Track.ProgressionTrackId = FName(*TrackId);
        Track.Level = static_cast<int32>(Level);
        Track.MaxLevel = static_cast<int32>(MaxLevel);
        Track.CurveVersion = static_cast<int32>(CurveVersion);

        if (Track.SubjectType ==
                EGamePlatformProgressionSubjectType::Unknown ||
            Track.SubjectId.IsEmpty() ||
            Track.ProgressionTrackId.IsNone() ||
            Track.TotalXP < 0 ||
            Track.Level < 1 ||
            Track.MaxLevel < Track.Level ||
            Track.CurveVersion < 1 ||
            Track.Revision < 1)
        {
            return false;
        }

        OutSnapshot.Tracks.Add(MoveTemp(Track));
    }

    return true;
}

bool FGamePlatformProgressionGatewayHttpTransport::ParseInt64String(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    int64& OutValue)
{
    FString Value;
    if (!Json.IsValid() ||
        !Json->TryGetStringField(Field, Value) ||
        Value.IsEmpty())
    {
        return false;
    }

    TCHAR* End = nullptr;
    const int64 Parsed = FCString::Strtoi64(*Value, &End, 10);
    if (!End || *End != TEXT('\0'))
    {
        return false;
    }

    OutValue = Parsed;
    return true;
}

EGamePlatformProgressionError
FGamePlatformProgressionGatewayHttpTransport::MapHttpError(
    int32 StatusCode)
{
    if (StatusCode == 401 || StatusCode == 403)
    {
        return EGamePlatformProgressionError::Unauthorized;
    }
    if (StatusCode == 0 || StatusCode >= 500)
    {
        return EGamePlatformProgressionError::BackendUnavailable;
    }
    return EGamePlatformProgressionError::OutcomeUnknown;
}
