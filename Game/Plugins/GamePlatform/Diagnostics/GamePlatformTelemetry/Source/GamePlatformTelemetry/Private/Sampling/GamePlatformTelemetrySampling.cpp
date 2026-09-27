#include "Sampling/GamePlatformTelemetrySampling.h"

#include "Misc/Crc.h"

bool FGamePlatformTelemetrySampler::ShouldSample(
    EGamePlatformTelemetrySamplingPolicy Policy,
    double SamplingRate,
    const FString& SamplingSeed,
    const FString& StableSessionSamplingKey,
    FName DefinitionName,
    const FGuid& RecordId)
{
    switch (Policy)
    {
    case EGamePlatformTelemetrySamplingPolicy::Always:
        return true;

    case EGamePlatformTelemetrySamplingPolicy::Disabled:
        return false;

    case EGamePlatformTelemetrySamplingPolicy::DeterministicSessionSample:
    {
        if (SamplingRate <= 0.0)
        {
            return false;
        }
        if (SamplingRate >= 1.0)
        {
            return true;
        }

        const FString Key =
            SamplingSeed +
            TEXT("|") +
            StableSessionSamplingKey +
            TEXT("|") +
            DefinitionName.ToString();

        const uint32 Hash = FCrc::StrCrc32(*Key);
        const double Unit =
            static_cast<double>(Hash) /
            static_cast<double>(MAX_uint32);

        return Unit < SamplingRate;
    }

    case EGamePlatformTelemetrySamplingPolicy::Probabilistic:
    {
        if (SamplingRate <= 0.0)
        {
            return false;
        }
        if (SamplingRate >= 1.0)
        {
            return true;
        }

        const FString Key =
            SamplingSeed +
            TEXT("|") +
            RecordId.ToString(EGuidFormats::Digits);

        const uint32 Hash = FCrc::StrCrc32(*Key);
        const double Unit =
            static_cast<double>(Hash) /
            static_cast<double>(MAX_uint32);

        return Unit < SamplingRate;
    }

    default:
        return false;
    }
}

bool FGamePlatformTelemetryRateLimiter::TryConsume(
    FName EventName,
    int32 SustainedRatePerSecond,
    int32 Burst,
    double NowSeconds)
{
    if (EventName.IsNone() ||
        SustainedRatePerSecond <= 0 ||
        Burst <= 0)
    {
        return false;
    }

    FScopeLock Lock(&Mutex);
    FBucket& Bucket = Buckets.FindOrAdd(EventName);

    if (!Bucket.bInitialized)
    {
        Bucket.Tokens = static_cast<double>(Burst);
        Bucket.LastRefillSeconds = NowSeconds;
        Bucket.bInitialized = true;
    }

    const double Elapsed =
        FMath::Max(0.0, NowSeconds - Bucket.LastRefillSeconds);

    Bucket.Tokens =
        FMath::Min(
            static_cast<double>(Burst),
            Bucket.Tokens +
                Elapsed *
                static_cast<double>(SustainedRatePerSecond));

    Bucket.LastRefillSeconds = NowSeconds;

    if (Bucket.Tokens < 1.0)
    {
        return false;
    }

    Bucket.Tokens -= 1.0;
    return true;
}

void FGamePlatformTelemetryRateLimiter::Reset()
{
    FScopeLock Lock(&Mutex);
    Buckets.Reset();
}
