#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXHandle.generated.h"

/** 世界内一次 VFX 实例的安全句柄。Generation 防止旧世界句柄误操作新世界实例。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid InstanceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    int32 Generation = 0;

    bool IsValid() const { return InstanceId.IsValid() && Generation > 0; }
    void Reset() { InstanceId.Invalidate(); Generation = 0; }
};
