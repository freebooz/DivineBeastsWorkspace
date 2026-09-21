#include "Validation/GamePlatformVFXDependencyValidator.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Catalogs/GamePlatformVFXCatalog.h"

bool UGamePlatformVFXDependencyValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject, FDataValidationContext&) const
{
    return InObject && (InObject->IsA<UGamePlatformVFXDefinition>() || InObject->IsA<UGamePlatformVFXCatalog>());
}

EDataValidationResult UGamePlatformVFXDependencyValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* InAsset, FDataValidationContext&)
{
    // 独立包不知道项目层真实 Asset Mount Point，因此不硬编码 /DivineBeasts/ 或 /MobaCommon/。
    // 接入工程后应使用 AssetRegistry 获取 Package Dependencies，并按仓库真实挂载点配置禁止列表。
    return EDataValidationResult::NotValidated;
}
