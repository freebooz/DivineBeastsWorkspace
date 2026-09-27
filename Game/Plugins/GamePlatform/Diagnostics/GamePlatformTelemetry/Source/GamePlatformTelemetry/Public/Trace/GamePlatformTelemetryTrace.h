#pragma once

#include "CoreMinimal.h"

class GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryTrace
{
public:
    static void EmitEvent(
        FName EventName,
        uint64 Sequence,
        const FString& CorrelationId);

    static void EmitBookmark(
        const FString& Name,
        const FString& CorrelationId);
};
