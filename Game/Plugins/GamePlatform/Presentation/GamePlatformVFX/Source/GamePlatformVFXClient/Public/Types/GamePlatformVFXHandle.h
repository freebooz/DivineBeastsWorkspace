#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXHandle.generated.h"

class UWorld;

/** VFX 活动实例句柄。Id 标识实例，Generation 防止旧句柄误操作复用实例。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid Id;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    int32 Generation = 0;

    /** 弱世界身份用于阻止跨World/Multi-PIE误操作，不延长World生命周期。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<UWorld> World;

    bool IsValid() const { return Id.IsValid() && Generation > 0; }
    bool BelongsToWorld(const UWorld* InWorld) const { return World.Get() == InWorld; }

    friend bool operator==(const FGamePlatformVFXHandle& A, const FGamePlatformVFXHandle& B)
    {
        return A.Id == B.Id && A.Generation == B.Generation && A.World == B.World;
    }
};
