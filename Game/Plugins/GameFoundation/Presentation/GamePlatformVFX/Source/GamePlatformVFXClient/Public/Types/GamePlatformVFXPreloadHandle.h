#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXPreloadHandle.generated.h"

/** VFX 预加载租约句柄。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXPreloadHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid RequestId;

    bool IsValid() const { return RequestId.IsValid(); }
};
