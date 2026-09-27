#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformProgressionTypes.h"
#include "GamePlatformProgressionTrackDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMPROGRESSION_API UGamePlatformProgressionTrackDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Character;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 CurveVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 MinLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 MaxLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    TArray<int64> CumulativeXPThresholds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionPostMaxXPPolicy PostMaxXPPolicy =
        EGamePlatformProgressionPostMaxXPPolicy::ClampAtMax;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 Version = 1;

    bool Validate(FString& OutReason) const;
    int32 CalculateLevel(int64 TotalXP) const;
    int64 GetMaxTotalXP() const;
    int64 CalculateXPIntoLevel(int64 TotalXP) const;
    int64 CalculateXPForNextLevel(int64 TotalXP) const;
};
