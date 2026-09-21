#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GamePlatformVFXDependencyValidator.generated.h"

/**
 * 跨层依赖验证器骨架。
 * 真实禁用路径需要读取神兽联盟仓库实际插件挂载点后配置，不能在独立源码包中臆造。
 */
UCLASS()
class UGamePlatformVFXDependencyValidator : public UEditorValidatorBase
{
    GENERATED_BODY()

protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
