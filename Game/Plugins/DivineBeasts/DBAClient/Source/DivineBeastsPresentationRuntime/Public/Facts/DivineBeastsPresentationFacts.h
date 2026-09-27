#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationTypes.h"
#include "DivineBeastsPresentationFacts.generated.h"

/** OpenWorld/Village InteractionCommitted（交互提交）表现事实。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsWorldInteractionPresentationFact
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FactId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OptionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WorldId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ExperienceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationPredictionState PredictionState =
        EGamePlatformPresentationPredictionState::Confirmed;

    bool IsValid() const { return FactId.IsValid() && !OptionId.IsNone(); }
};

/** Village Tutorial/Training表现反馈事实。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsVillageFeedbackPresentationFact
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid FactId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName FeedbackId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WorldId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ExperienceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationPredictionState PredictionState =
        EGamePlatformPresentationPredictionState::Confirmed;

    bool IsValid() const;
};
