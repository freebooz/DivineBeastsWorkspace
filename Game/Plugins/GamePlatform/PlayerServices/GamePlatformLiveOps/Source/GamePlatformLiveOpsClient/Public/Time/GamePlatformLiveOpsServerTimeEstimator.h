#pragma once

#include "CoreMinimal.h"

class GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsServerTimeEstimator
{
public:
    void Reset();
    void Update(const FDateTime& ServerTimeUtc);
    bool IsValid() const { return bValid; }
    FDateTime EstimatedServerNowUtc() const;

private:
    FDateTime ServerTimeUtc;
    double MonotonicSecondsAtSnapshot = 0.0;
    bool bValid = false;
};
