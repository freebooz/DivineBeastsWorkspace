#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IGamePlatformPCGFieldProvider.generated.h"

UINTERFACE(BlueprintType)
class GAMEPLATFORMPCG_API UGamePlatformPCGFieldProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * IGamePlatformPCGFieldProvider（PCG场数据提供者）。
 * 1.0仅定义只读采样合同；Landscape、权重图或项目自定义场可以在上层适配，不允许反向写地形。
 */
class GAMEPLATFORMPCG_API IGamePlatformPCGFieldProvider
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GamePlatform|PCG|Field")
    bool SampleHeight(const FVector& WorldPosition, float& OutHeightCm) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GamePlatform|PCG|Field")
    bool SampleScalarField(FName FieldId, const FVector& WorldPosition, float& OutValue) const;
};
