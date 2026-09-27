#include "Time/GamePlatformLiveOpsServerTimeEstimator.h"

#include "HAL/PlatformTime.h"

void FGamePlatformLiveOpsServerTimeEstimator::Reset()
{
    ServerTimeUtc = {};
    MonotonicSecondsAtSnapshot = 0.0;
    bValid = false;
}

void FGamePlatformLiveOpsServerTimeEstimator::Update(
    const FDateTime& InServerTimeUtc)
{
    if (InServerTimeUtc.GetTicks() <= 0)
    {
        return;
    }

    ServerTimeUtc = InServerTimeUtc;
    MonotonicSecondsAtSnapshot = FPlatformTime::Seconds();
    bValid = true;
}

FDateTime FGamePlatformLiveOpsServerTimeEstimator::EstimatedServerNowUtc() const
{
    if (!bValid)
    {
        return {};
    }

    const double Elapsed =
        FMath::Max(
            0.0,
            FPlatformTime::Seconds() -
                MonotonicSecondsAtSnapshot);

    return ServerTimeUtc +
           FTimespan::FromSeconds(Elapsed);
}
