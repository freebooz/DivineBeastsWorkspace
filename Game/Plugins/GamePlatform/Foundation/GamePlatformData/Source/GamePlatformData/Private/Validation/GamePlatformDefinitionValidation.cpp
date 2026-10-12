// 平台层稳定唯一源接口的内部转发；只访问模块私有元数据索引，不拥有加载/租约/世界。
// 参数、错误码和EditorCandidate替换语义保持不变，AssetManager映射核对仍在原调用方执行。
#include "Validation/GamePlatformDefinitionValidation.h"
#include "Validation/GamePlatformDefinitionSourceIndex.h"

FGamePlatformResult ResolveUniqueGamePlatformDefinitionSource(const FPrimaryAssetId& DefinitionId,
    FSoftObjectPath& OutSource, const UGamePlatformPrimaryDataAsset* EditorCandidate)
{
    return GamePlatform::Data::GetDefinitionSourceIndex().Resolve(DefinitionId, OutSource, EditorCandidate);
}
