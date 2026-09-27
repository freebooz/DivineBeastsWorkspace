#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXPreloadHandle.generated.h"

/** 异步预加载句柄。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXPreloadHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};
