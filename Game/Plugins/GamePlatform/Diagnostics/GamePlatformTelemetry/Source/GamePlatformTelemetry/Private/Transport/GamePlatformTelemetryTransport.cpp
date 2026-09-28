#include "Transport/GamePlatformTelemetryTransport.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
TSharedPtr<FJsonObject> ContextJson(
    const FGamePlatformTelemetryContext& Context)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

    const auto SetIf =
        [&Json](const TCHAR* Key, const FString& Value)
        {
            if (!Value.IsEmpty())
            {
                Json->SetStringField(Key, Value);
            }
        };

    SetIf(TEXT("build_version"), Context.BuildVersion);
    SetIf(TEXT("content_revision"), Context.ContentRevision);
    SetIf(TEXT("platform"), Context.Platform);
    SetIf(TEXT("environment"), Context.Environment);
    SetIf(TEXT("source_role"), Context.SourceRole);
    SetIf(TEXT("server_role"), Context.ServerRole);
    SetIf(TEXT("region"), Context.Region);
    SetIf(TEXT("map_id"), Context.MapId);
    SetIf(TEXT("world_id"), Context.WorldId);
    SetIf(TEXT("experience_id"), Context.ExperienceId);
    SetIf(TEXT("server_instance_id"), Context.ServerInstanceId);
    SetIf(TEXT("match_id"), Context.MatchId);
    SetIf(TEXT("arena_mode_id"), Context.ArenaModeId);
    SetIf(TEXT("session_id"), Context.SessionId);
    SetIf(
        TEXT("pseudonymous_player_id"),
        Context.PseudonymousPlayerId);
    SetIf(TEXT("correlation_id"), Context.CorrelationId);
    SetIf(TEXT("transaction_id"), Context.TransactionId);
    return Json;
}

TSharedPtr<FJsonObject> AttributeJson(
    const FGamePlatformTelemetryAttribute& Attribute)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("key"), Attribute.Key.ToString());

    switch (Attribute.Type)
    {
    case EGamePlatformTelemetryAttributeType::String:
        Json->SetStringField(TEXT("type"), TEXT("string"));
        Json->SetStringField(TEXT("string_value"), Attribute.StringValue);
        break;
    case EGamePlatformTelemetryAttributeType::Int64:
        Json->SetStringField(TEXT("type"), TEXT("int64"));
        Json->SetStringField(
            TEXT("int64_value"),
            LexToString(Attribute.Int64Value));
        break;
    case EGamePlatformTelemetryAttributeType::Double:
        Json->SetStringField(TEXT("type"), TEXT("double"));
        Json->SetNumberField(
            TEXT("double_value"),
            Attribute.DoubleValue);
        break;
    case EGamePlatformTelemetryAttributeType::Bool:
        Json->SetStringField(TEXT("type"), TEXT("bool"));
        Json->SetBoolField(
            TEXT("bool_value"),
            Attribute.BoolValue);
        break;
    default:
        break;
    }

    return Json;
}

FString PriorityString(EGamePlatformTelemetryPriority Priority)
{
    switch (Priority)
    {
    case EGamePlatformTelemetryPriority::Verbose:
        return TEXT("verbose");
    case EGamePlatformTelemetryPriority::Normal:
        return TEXT("normal");
    case EGamePlatformTelemetryPriority::CriticalTelemetry:
        return TEXT("critical_telemetry");
    default:
        return TEXT("normal");
    }
}

FString MetricTypeString(EGamePlatformTelemetryMetricType Type)
{
    switch (Type)
    {
    case EGamePlatformTelemetryMetricType::Counter:
        return TEXT("counter");
    case EGamePlatformTelemetryMetricType::Gauge:
        return TEXT("gauge");
    case EGamePlatformTelemetryMetricType::Histogram:
        return TEXT("histogram");
    case EGamePlatformTelemetryMetricType::Duration:
        return TEXT("duration");
    default:
        return TEXT("unknown");
    }
}
}

FGamePlatformTelemetryHttpTransport::
FGamePlatformTelemetryHttpTransport(
    FString InBaseUrl,
    FString InPath,
    TMap<FString, FString> InStaticHeaders,
    float InTimeoutSeconds,
    int32 InMaxPayloadBytes,
    FGamePlatformTelemetryHeaderProvider InDynamicHeaderProvider)
    : BaseUrl(MoveTemp(InBaseUrl))
    , Path(MoveTemp(InPath))
    , StaticHeaders(MoveTemp(InStaticHeaders))
    , DynamicHeaderProvider(MoveTemp(InDynamicHeaderProvider))
    , MaxPayloadBytes(FMath::Max(1024, InMaxPayloadBytes))
    , TimeoutSeconds(FMath::Max(1.0f, InTimeoutSeconds))
{
    BaseUrl.RemoveFromEnd(TEXT("/"));
    if (!Path.StartsWith(TEXT("/")))
    {
        Path = TEXT("/") + Path;
    }
}

bool FGamePlatformTelemetryHttpTransport::IsConfigured() const
{
    if (BaseUrl.IsEmpty() || Path.IsEmpty())
    {
        return false;
    }
#if UE_BUILD_SHIPPING
    // Shipping携带玩家/服务器凭据时必须使用TLS；开发环境仍可对本地HTTP进行联调。
    return BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#else
    return BaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ||
        BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#endif
}

bool FGamePlatformTelemetryHttpTransport::BeginSubmitBatch(
    const FGamePlatformTelemetryBatch& Batch,
    FGamePlatformTelemetryTransportCompletion Completion)
{
    if (!IsConfigured() ||
        !Batch.BatchId.IsValid() ||
        !Completion)
    {
        return false;
    }

    FString Payload;
    if (!SerializeBatch(Batch, Payload))
    {
        return false;
    }

    FTCHARToUTF8 Utf8Payload(*Payload);
    if (Utf8Payload.Length() > MaxPayloadBytes)
    {
        // 超大批次是不可重试的本地结构问题；通过Completion返回明确失败，避免NetworkSink指数重试同一坏Batch。
        AsyncTask(ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion)]() mutable
            {
                FGamePlatformTelemetryTransportResult Result;
                Result.bAccepted = false;
                Result.bRetryable = false;
                Result.Error = TEXT("payload_too_large");
                Completion(Result);
            });
        return true;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> RequestPtr = Request;

    Request->SetURL(BaseUrl + Path);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json"));
    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json"));
    Request->SetTimeout(TimeoutSeconds);

    for (const TPair<FString, FString>& Header : StaticHeaders)
    {
        if (!Header.Key.IsEmpty() && !Header.Value.IsEmpty())
        {
            Request->SetHeader(Header.Key, Header.Value);
        }
    }

    if (DynamicHeaderProvider)
    {
        const TMap<FString, FString> DynamicHeaders = DynamicHeaderProvider();
        for (const TPair<FString, FString>& Header : DynamicHeaders)
        {
            if (!Header.Key.IsEmpty() && !Header.Value.IsEmpty() &&
                !Header.Key.Contains(TEXT("\r")) && !Header.Key.Contains(TEXT("\n")) &&
                !Header.Value.Contains(TEXT("\r")) && !Header.Value.Contains(TEXT("\n")))
            {
                Request->SetHeader(Header.Key, Header.Value);
            }
        }
    }

    Request->SetContentAsString(Payload);

    {
        FScopeLock Lock(&RequestsMutex);
        constexpr int32 MaxConcurrentHttpRequests = 8;
        if (ActiveRequests.Num() >= MaxConcurrentHttpRequests)
        {
            return false;
        }
        ActiveRequests.Add(RequestPtr);
    }

    TWeakPtr<
        FGamePlatformTelemetryHttpTransport,
        ESPMode::ThreadSafe> WeakThis = AsShared();

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis,
         RequestPtr,
         Completion = MoveTemp(Completion)](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bConnectedSuccessfully) mutable
        {
            if (TSharedPtr<
                    FGamePlatformTelemetryHttpTransport,
                    ESPMode::ThreadSafe> Self =
                    WeakThis.Pin())
            {
                Self->UnregisterRequest(RequestPtr);
            }

            FGamePlatformTelemetryTransportResult Result;

            if (!bConnectedSuccessfully || !Response.IsValid())
            {
                Result.bRetryable = true;
                Result.Error = TEXT("transport_unavailable");
            }
            else
            {
                const int32 Code = Response->GetResponseCode();
                Result.bAccepted = Code >= 200 && Code < 300;
                Result.bRetryable =
                    Code == 408 ||
                    Code == 429 ||
                    Code >= 500;

                if (Code == 429)
                {
                    const FString RetryAfter =
                        Response->GetHeader(TEXT("Retry-After"));

                    if (!RetryAfter.IsEmpty())
                    {
                        Result.RetryAfterSeconds =
                            FCString::Atof(*RetryAfter);
                    }
                }

                if (!Result.bAccepted)
                {
                    Result.Error =
                        FString::Printf(
                            TEXT("http_%d"),
                            Code);
                }
            }

            AsyncTask(
                ENamedThreads::GameThread,
                [Completion = MoveTemp(Completion),
                 Result]() mutable
                {
                    Completion(Result);
                });
        });

    const bool bStarted = Request->ProcessRequest();
    if (!bStarted)
    {
        UnregisterRequest(RequestPtr);
    }

    return bStarted;
}

void FGamePlatformTelemetryHttpTransport::CancelAll()
{
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> Requests;
    {
        FScopeLock Lock(&RequestsMutex);
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

void FGamePlatformTelemetryHttpTransport::UnregisterRequest(
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    FScopeLock Lock(&RequestsMutex);
    ActiveRequests.Remove(Request);
}

bool FGamePlatformTelemetryHttpTransport::SerializeBatch(
    const FGamePlatformTelemetryBatch& Batch,
    FString& OutJson)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("batch_id"),
        Batch.BatchId.ToString(EGuidFormats::DigitsWithHyphensLower));
    Root->SetNumberField(TEXT("schema_version"), Batch.SchemaVersion);
    Root->SetStringField(
        TEXT("created_at_utc"),
        Batch.CreatedAtUtc.ToIso8601());
    Root->SetStringField(
        TEXT("dropped_since_last_batch"),
        LexToString(Batch.DroppedSinceLastBatch));
    Root->SetObjectField(
        TEXT("source_context"),
        ContextJson(Batch.SourceContext));

    TArray<TSharedPtr<FJsonValue>> EventValues;
    EventValues.Reserve(Batch.Events.Num());

    for (const FGamePlatformTelemetryEvent& Event : Batch.Events)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(
            TEXT("event_id"),
            Event.EventId.ToString(EGuidFormats::DigitsWithHyphensLower));
        Json->SetStringField(
            TEXT("event_name"),
            Event.EventName.ToString());
        Json->SetNumberField(
            TEXT("schema_version"),
            Event.SchemaVersion);
        Json->SetStringField(
            TEXT("timestamp_utc"),
            Event.TimestampUtc.ToIso8601());
        Json->SetNumberField(
            TEXT("monotonic_timestamp"),
            Event.MonotonicTimestampSeconds);
        Json->SetStringField(
            TEXT("sequence"),
            LexToString(Event.Sequence));
        Json->SetStringField(
            TEXT("priority"),
            PriorityString(Event.Priority));

        TArray<TSharedPtr<FJsonValue>> Attributes;
        Attributes.Reserve(Event.Attributes.Num());

        for (const FGamePlatformTelemetryAttribute& Attribute :
             Event.Attributes)
        {
            Attributes.Add(
                MakeShared<FJsonValueObject>(
                    AttributeJson(Attribute)));
        }

        Json->SetArrayField(TEXT("attributes"), MoveTemp(Attributes));
        EventValues.Add(MakeShared<FJsonValueObject>(Json));
    }

    Root->SetArrayField(TEXT("events"), MoveTemp(EventValues));

    TArray<TSharedPtr<FJsonValue>> MetricValues;
    MetricValues.Reserve(Batch.Metrics.Num());

    for (const FGamePlatformTelemetryMetric& Metric : Batch.Metrics)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Metric.Name.ToString());
        Json->SetStringField(
            TEXT("type"),
            MetricTypeString(Metric.Type));
        Json->SetStringField(TEXT("unit"), Metric.Unit);
        Json->SetNumberField(TEXT("value"), Metric.Value);
        Json->SetStringField(
            TEXT("timestamp_utc"),
            Metric.TimestampUtc.ToIso8601());

        TSharedPtr<FJsonObject> Labels = MakeShared<FJsonObject>();
        for (const TPair<FName, FString>& Label : Metric.Labels)
        {
            Labels->SetStringField(
                Label.Key.ToString(),
                Label.Value);
        }
        Json->SetObjectField(TEXT("labels"), Labels);
        MetricValues.Add(MakeShared<FJsonValueObject>(Json));
    }

    Root->SetArrayField(TEXT("metrics"), MoveTemp(MetricValues));

    TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutJson);

    return FJsonSerializer::Serialize(
        Root.ToSharedRef(),
        Writer);
}
