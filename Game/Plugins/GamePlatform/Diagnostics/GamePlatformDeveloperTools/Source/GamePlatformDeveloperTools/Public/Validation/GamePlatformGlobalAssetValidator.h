#pragma once

#include "CoreMinimal.h"
#include "Validation/GamePlatformValidationTypes.h"

struct FAssetData;

/**
 * FGamePlatformGlobalAssetValidator（平台全局资产验证器）。
 * 用于必须跨资产聚合才能判断的规则；Editor游戏线程按需调用，不是运行时逐帧服务。
 * 从AssetRegistry继承集合筛选真实平台定义，再只读加载定义校验LogicalId、版本与必需依赖；不持有运行租约。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformGlobalAssetValidator
{
public:
    /** 在现有结果后追加项目定义/身份/引用结果；资产加载或契约失败写入Failed，不能由名称猜测成功。 */
    static void ValidateProject(
        TArray<FGamePlatformValidationResult>& OutResults);

private:
    friend class FGamePlatformDefinitionIdentityAuditTest;
    static void ValidateDefinitionsAndStableIds(
        const TArray<FAssetData>& ProjectAssets,
        TArray<FGamePlatformValidationResult>& OutResults);

    static void ValidatePrimaryAssetsChunksAndReferences(
        const TArray<FAssetData>& ProjectAssets,
        TArray<FGamePlatformValidationResult>& OutResults);
};
