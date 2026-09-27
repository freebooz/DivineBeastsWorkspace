#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryTransportResult
{
    bool bAccepted = false;
    bool bRetryable = false;
    float RetryAfterSeconds = 0.0f;
    FString Error;
};

using FGamePlatformTelemetryTransportCompletion =
    TFunction<void(FGamePlatformTelemetryTransportResult)>;

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
        float InTimeoutSeconds = 5.0f);

    bool IsConfigured() const;

    virtual bool BeginSubmitBatch(
        const FGamePlatformTelemetryBatch& Batch,
        FGamePlatformTelemetryTransportCompletion Completion) override;

    virtual void CancelAll() override;

private:
    FString BaseUrl;
    FString Path;
    TMap<FString, FString> StaticHeaders;
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
