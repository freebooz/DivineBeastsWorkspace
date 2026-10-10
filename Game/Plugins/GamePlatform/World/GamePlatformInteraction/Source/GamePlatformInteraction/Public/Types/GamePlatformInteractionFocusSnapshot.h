#pragma once

#include "CoreMinimal.h"
// 公开焦点有效性内联需确认Actor到UObject继承转换，直接包含完整Actor类型。
#include "GameFramework/Actor.h"
#include "Types/GamePlatformInteractionOption.h"
#include "GamePlatformInteractionFocusSnapshot.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMINTERACTION_API FGamePlatformInteractionFocusSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGuid TargetInstanceId;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetGeneration = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    int32 TargetRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    FGamePlatformInteractionOption Option;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    float Distance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Interaction")
    bool bLocallyAvailable = false;

    /** 游戏线程只读焦点形状；本地有效不等于服务器许可，Target失效立即为false。 */
    bool IsValid() const
    {
        return ::IsValid(TargetActor.Get()) &&
               TargetInstanceId.IsValid() &&
               !Option.OptionId.IsNone();
    }
};
