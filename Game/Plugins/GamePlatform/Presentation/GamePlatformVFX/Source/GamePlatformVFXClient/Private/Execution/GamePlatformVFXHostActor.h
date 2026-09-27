#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GamePlatformVFXHostActor.generated.h"

/** 仅复杂独立空间 VFX 使用；普通 VFX 应优先直接使用 UNiagaraComponent。 */
UCLASS(NotBlueprintable, Transient)
class AGamePlatformVFXHostActor : public AActor
{
    GENERATED_BODY()

public:
    AGamePlatformVFXHostActor();
};
