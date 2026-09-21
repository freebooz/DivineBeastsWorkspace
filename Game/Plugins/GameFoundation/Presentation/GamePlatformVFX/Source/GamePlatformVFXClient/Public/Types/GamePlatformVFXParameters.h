#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXParameters.generated.h"

/**
 * 通用 Niagara User Parameter 参数集合。
 * 参数名建议统一使用 User.VFX.* 命名空间。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, float> FloatValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, int32> IntValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, FVector> VectorValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, FLinearColor> ColorValues;

    void Append(const FGamePlatformVFXParameters& Other);
};
