#pragma once

#include "CoreMinimal.h"

class UMaterialParameterCollection;

/**
 * GamePlatformSurface通用材质资产契约。
 *
 * 本类型只描述稳定路径、参数与资产职责，不把“文档中存在路径”冒充真实uasset。
 * 真正资产必须由Unreal Editor/Commandlet创建并由AssetRegistry回读验证。
 */
struct GAMEPLATFORMSURFACEEDITOR_API FGamePlatformSurfaceAssetContract
{
    /** 标准全局MPC包名，不包含“.AssetName”对象后缀。 */
    static const FString& GetGlobalParameterCollectionPackageName();

    /** 标准MPC对象软路径。 */
    static FSoftObjectPath GetGlobalParameterCollectionObjectPath();

    /** 标准MPC中必须存在的8个标量参数及默认值。 */
    static const TArray<TPair<FName, float>>& GetRequiredScalarParameters();

    /** 一期必须由材质编辑器真实制作的通用母材质/材质函数包名。 */
    static const TArray<FString>& GetRequiredMaterialAuthoringPackages();

    /** 二期或按场景需要创建的扩展材质包名。 */
    static const TArray<FString>& GetOptionalMaterialAuthoringPackages();

    /** 验证MPC参数名称与类型契约；不依赖项目专属纹理。 */
    static bool ValidateGlobalParameterCollection(
        const UMaterialParameterCollection& Collection,
        FString& OutError);
};
