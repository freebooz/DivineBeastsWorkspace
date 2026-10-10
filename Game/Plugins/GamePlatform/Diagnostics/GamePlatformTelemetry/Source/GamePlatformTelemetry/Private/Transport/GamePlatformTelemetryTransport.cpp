#include "Transport/GamePlatformTelemetryTransport.h"
#include "Transport/TelemetryResponseBudget.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeCounter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
/** 每请求独立接收账本；HTTP线程只访问该账本，不触碰UObject或传输器生命周期。 */
struct FTelemetryResponseBudget
{
    FCriticalSection Mutex;
    int64 ReceivedBytes = 0;
    bool bOverflow = false;
};
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
    FGamePlatformTelemetryHeaderProvider InDynamicHeaderProvider,
    FGamePlatformTelemetryRequestAuthorizer InRequestAuthorizer)
    : BaseUrl(MoveTemp(InBaseUrl))
    , Path(MoveTemp(InPath))
    , StaticHeaders(MoveTemp(InStaticHeaders))
    , DynamicHeaderProvider(MoveTemp(InDynamicHeaderProvider))
    , RequestAuthorizer(MoveTemp(InRequestAuthorizer))
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
    check(IsInGameThread());
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

#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    // 遥测可能携带认证上下文；自动重定向必须显式关闭，未知HTTP后端Fail Closed。
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        return false;
    }
#else
    return false;
#endif

    Request->SetURL(BaseUrl + Path);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json"));
    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json"));
    Request->SetTimeout(TimeoutSeconds);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    const auto ResponseBudget = MakeShared<FTelemetryResponseBudget, ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 Receive = FHttpRequestStreamDelegateV2::CreateLambda(
        [ResponseBudget, LimitBytes = MaxPayloadBytes](void* Data, int64& InOutLength)
        {
            FScopeLock Lock(&ResponseBudget->Mutex);
            if (ResponseBudget->bOverflow || (InOutLength > 0 && !Data) ||
                !GamePlatform::Telemetry::CanReceiveBytes(ResponseBudget->ReceivedBytes, InOutLength, LimitBytes))
            { ResponseBudget->bOverflow = true; InOutLength = 0; return; }
            // 遥测只消费HTTP状态与Retry-After，不保存正文；仍限制累计接收字节以阻止无限确认响应。
            ResponseBudget->ReceivedBytes += InOutLength;
        });
    if (!Request->SetResponseBodyReceiveStreamDelegateV2(MoveTemp(Receive))) return false;

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

    if (RequestAuthorizer && !RequestAuthorizer(*Request))
    {
        // 未认证时不发送裸遥测请求；上层NetworkSink可按既有策略重试或丢弃。
        return false;
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

    const auto CompletionGate = MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();
    const auto SharedCompletion = MakeShared<FGamePlatformTelemetryTransportCompletion, ESPMode::ThreadSafe>(MoveTemp(Completion));
    const auto CompleteOnce = [SharedCompletion, CompletionGate](FGamePlatformTelemetryTransportResult Result)
    {
        if (CompletionGate->Increment() != 1) return;
        AsyncTask(ENamedThreads::GameThread, [SharedCompletion, Result = MoveTemp(Result)]() mutable
        { if (*SharedCompletion) (*SharedCompletion)(MoveTemp(Result)); });
    };
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis,
         ResponseBudget,
         CompleteOnce](
            FHttpRequestPtr CompletedRequest,
            FHttpResponsePtr Response,
            bool bConnectedSuccessfully) mutable
        {
            if (TSharedPtr<
                    FGamePlatformTelemetryHttpTransport,
                    ESPMode::ThreadSafe> Self =
                    WeakThis.Pin())
            {
                // 使用回调参数，不捕获Request自身；避免Request→委托→Request强引用环。
                Self->UnregisterRequest(CompletedRequest);
            }

            FGamePlatformTelemetryTransportResult Result;

            bool bOverflow = false;
            { FScopeLock Lock(&ResponseBudget->Mutex); bOverflow = ResponseBudget->bOverflow; }
            if (bOverflow)
            {
                Result.bRetryable = false;
                Result.Error = TEXT("response_too_large");
            }
            else if (!bConnectedSuccessfully || !Response.IsValid())
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

            CompleteOnce(MoveTemp(Result));
        });

    const bool bStarted = Request->ProcessRequest();
    if (!bStarted)
    {
        Request->OnProcessRequestComplete().Unbind();
        UnregisterRequest(RequestPtr);
        FGamePlatformTelemetryTransportResult Result;
        Result.bRetryable = true;
        Result.Error = TEXT("request_start_failed");
        CompleteOnce(MoveTemp(Result));
    }

    // 安装完成协议后即受理；启动失败也由同一个门闩回调完成，避免上层同时处理false和迟到回调。
    return true;
}

void FGamePlatformTelemetryHttpTransport::CancelAll()
{
    check(IsInGameThread());
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
