#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

class IHttpRequest;

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryTransportResult
{
    bool bAccepted = false;
    bool bRetryable = false;
    float RetryAfterSeconds = 0.0f;
    FString Error;
};

using FGamePlatformTelemetryTransportCompletion =
    TFunction<void(FGamePlatformTelemetryTransportResult)>;

/** 每次发送前动态生成非持久请求头；用于令牌轮换，Transport不长期保存AccessToken。 */
using FGamePlatformTelemetryHeaderProvider =
    TFunction<TMap<FString, FString>()>;

/** 在请求真正发送前应用动态认证，但不要求调用方暴露或复制原始Token字符串。 */
using FGamePlatformTelemetryRequestAuthorizer =
    TFunction<bool(IHttpRequest&)>;

class GAMEPLATFORMTELEMETRY_API IGamePlatformTelemetryTransport
{
public:
    virtual ~IGamePlatformTelemetryTransport() = default;

    virtual bool BeginSubmitBatch(
        const FGamePlatformTelemetryBatch& Batch,
        FGamePlatformTelemetryTransportCompletion Completion) = 0;

    virtual void CancelAll() = 0;
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryHttpTransport final
    : public IGamePlatformTelemetryTransport
    , public TSharedFromThis<
        FGamePlatformTelemetryHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformTelemetryHttpTransport(
        FString InBaseUrl,
        FString InPath,
        TMap<FString, FString> InStaticHeaders,
        float InTimeoutSeconds = 5.0f,
        int32 InMaxPayloadBytes = 256 * 1024,
        FGamePlatformTelemetryHeaderProvider InDynamicHeaderProvider = {},
        FGamePlatformTelemetryRequestAuthorizer InRequestAuthorizer = {});

    bool IsConfigured() const;

    virtual bool BeginSubmitBatch(
        const FGamePlatformTelemetryBatch& Batch,
        FGamePlatformTelemetryTransportCompletion Completion) override;

    virtual void CancelAll() override;

private:
    FString BaseUrl;
    FString Path;
    TMap<FString, FString> StaticHeaders;
    FGamePlatformTelemetryHeaderProvider DynamicHeaderProvider;
    FGamePlatformTelemetryRequestAuthorizer RequestAuthorizer;
    int32 MaxPayloadBytes = 256 * 1024;
    float TimeoutSeconds = 5.0f;

    FCriticalSection RequestsMutex;
    TArray<TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>>
        ActiveRequests;

    static bool SerializeBatch(
        const FGamePlatformTelemetryBatch& Batch,
        FString& OutJson);

    void UnregisterRequest(
        const TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>& Request);
};
