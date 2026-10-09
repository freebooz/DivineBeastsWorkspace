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

    /** 平台中立目标位置，供Beam/Projectile等Niagara约定参数使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector TargetLocation = FVector::ZeroVector;

    /** 命中位置与法线；只承载表现上下文，不参与Gameplay判定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector ImpactLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector ImpactNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FVector Scale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TWeakObjectPtr<USceneComponent> AttachComponent;
    // 弱目标不延长Owner生命周期；异步完成和每个延迟子步骤在执行器再次核对Owner及World。

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName AttachPointName = NAME_None;
};
