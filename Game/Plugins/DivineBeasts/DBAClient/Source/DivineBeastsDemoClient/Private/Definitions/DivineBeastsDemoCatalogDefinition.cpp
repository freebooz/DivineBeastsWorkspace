// SSLM项目开发定义桥接：复用平台版本/身份检查，把纯目录结构验证留在模块Private。
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"
#include "Catalog/DivineBeastsDemoCatalogValidation.h"

UDivineBeastsDemoCatalogDefinition::UDivineBeastsDemoCatalogDefinition()
{
    DataVersion.SchemaVersion = 2;
}

FGamePlatformResult UDivineBeastsDemoCatalogDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    return BaseResult.IsSuccess() ? DivineBeasts::Demo::ValidateCatalog(*this) : BaseResult;
}
