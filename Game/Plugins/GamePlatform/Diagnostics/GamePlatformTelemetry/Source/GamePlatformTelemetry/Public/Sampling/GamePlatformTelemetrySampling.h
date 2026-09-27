#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformTelemetryTypes.h"

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetrySampler
{
public:
    static bool ShouldSample(
        EGamePlatformTelemetrySamplingPolicy Policy,
        double SamplingRate,
        const FString& SamplingSeed,
        const FString& StableSessionSamplingKey,
        FName DefinitionName,
        const FGuid& RecordId);
};

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryRateLimiter
{
public:
    bool TryConsume(
        FName EventName,
        int32 SustainedRatePerSecond,
        int32 Burst,
        double NowSeconds);

    void Reset();

private:
    struct FBucket
    {
        double Tokens = 0.0;
        double LastRefillSeconds = 0.0;
        bool bInitialized = false;
    };

    FCriticalSection Mutex;
    TMap<FName, FBucket> Buckets;
};
