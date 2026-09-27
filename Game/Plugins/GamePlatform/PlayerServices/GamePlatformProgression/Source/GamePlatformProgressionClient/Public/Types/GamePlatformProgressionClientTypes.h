#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformProgressionTypes.h"
#include "GamePlatformProgressionClientTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformProgressionClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSIONCLIENT_API FGamePlatformProgressionViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FString SubjectId;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 MaxLevel = 1;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 TotalXP = 0;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 XPIntoLevel = 0;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 XPForNextLevel = 0;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    float ProgressPercent = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Progression")
    bool bCurveCompatible = false;
};
