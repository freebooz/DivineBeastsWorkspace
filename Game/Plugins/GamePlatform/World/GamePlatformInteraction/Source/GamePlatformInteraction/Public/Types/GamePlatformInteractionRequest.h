#pragma once

#include "CoreMinimal.h"
#include "GamePlatformInteractionRequest.generated.h"

class AActor;

/** 客户端只回传自己观察到的身份/修订；不携带距离、视线、时长或并发规则。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    FGuid RequestId;

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    FGuid TargetInstanceId;

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    int32 TargetGeneration = 0;

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    int32 ObservedTargetRevision = 0;

    UPROPERTY(BlueprintReadWrite, Category="Interaction")
    FName OptionId = NAME_None;
};
