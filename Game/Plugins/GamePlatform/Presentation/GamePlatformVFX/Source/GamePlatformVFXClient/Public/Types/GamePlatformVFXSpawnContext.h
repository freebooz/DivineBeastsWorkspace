#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXSpawnContext.generated.h"

class USceneComponent;

/** VFX 生成位置与附着上下文。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXSpawnContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector Scale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TObjectPtr<USceneComponent> AttachComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName AttachPointName = NAME_None;
};
