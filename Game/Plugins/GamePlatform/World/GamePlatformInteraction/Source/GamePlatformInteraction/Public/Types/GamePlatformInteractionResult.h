#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInteractionTypes.h"
#include "GamePlatformInteractionResult.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid SessionId;

    /** 最终提交对应的交互选项ID；用于客户端复制后确定性消费结果。 */
    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FName OptionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionError Error = EGamePlatformInteractionError::None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionSessionState State =
        EGamePlatformInteractionSessionState::None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    EGamePlatformInteractionCancelReason CancelReason =
        EGamePlatformInteractionCancelReason::None;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 FinalTargetRevision = 0;

    bool IsSuccess() const
    {
        return Error == EGamePlatformInteractionError::None &&
               State == EGamePlatformInteractionSessionState::Completed;
    }
};
