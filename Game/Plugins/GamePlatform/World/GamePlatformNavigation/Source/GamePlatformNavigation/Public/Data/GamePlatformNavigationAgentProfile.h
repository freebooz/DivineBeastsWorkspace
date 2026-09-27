#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformNavigationTypes.h"
#include "GamePlatformNavigationAgentProfile.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMNAVIGATION_API UGamePlatformNavigationAgentProfile final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    FName ProfileId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="1.0"))
    float AgentRadius = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="1.0"))
    float AgentHeight = 192.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0.0"))
    float StepHeight = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0.0", ClampMax="90.0"))
    float MaxSlope = 44.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    FName SupportedNavDataId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    FName DefaultFilterId = TEXT("Navigation.Filter.Default");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    EGamePlatformNavigationInvokerPolicy InvokerPolicy =
        EGamePlatformNavigationInvokerPolicy::Disabled;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0.0"))
    float TileGenerationRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="0.0"))
    float TileRemovalRadius = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    EGamePlatformNavigationPartialPathPolicy DefaultPartialPathPolicy =
        EGamePlatformNavigationPartialPathPolicy::RejectPartial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation", meta=(ClampMin="1"))
    int32 Version = 1;

    bool ValidateProfile(FText& OutReason) const;
};
