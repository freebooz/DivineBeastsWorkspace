#pragma once
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"

class UGamePlatformPrimaryDataAsset;
/**
 * 在独立源资产注册表扫描平台主资产及派生类，要求逻辑身份唯一，返回唯一物理路径。
 * 仅游戏线程；注册表未就绪、缺失、重复均显式失败。不会加载对象或使用去重后的主资产字典。
 * EditorCandidate用于编辑器未保存对象：以其当前身份替换同路径标签；运行期应传nullptr。
 */
GAMEPLATFORMDATA_API FGamePlatformResult ResolveUniqueGamePlatformDefinitionSource(
    const FPrimaryAssetId& DefinitionId, FSoftObjectPath& OutSource,
    const UGamePlatformPrimaryDataAsset* EditorCandidate = nullptr);
