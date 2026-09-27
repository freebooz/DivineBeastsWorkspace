#pragma once

#include "CoreMinimal.h"
#include "Validation/GamePlatformValidationTypes.h"

struct FAssetData;

/**
 * FGamePlatformGlobalAssetValidator（平台全局资产验证器）。
 * 用于必须跨资产聚合才能判断的规则；优先使用Asset Registry，仅目标加载Definition类资产。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformGlobalAssetValidator
{
public:
    static void ValidateProject(
        TArray<FGamePlatformValidationResult>& OutResults);

private:
    static void ValidateDefinitionsAndStableIds(
        const TArray<FAssetData>& ProjectAssets,
        TArray<FGamePlatformValidationResult>& OutResults);

    static void ValidatePrimaryAssetsChunksAndReferences(
        const TArray<FAssetData>& ProjectAssets,
        TArray<FGamePlatformValidationResult>& OutResults);
};
