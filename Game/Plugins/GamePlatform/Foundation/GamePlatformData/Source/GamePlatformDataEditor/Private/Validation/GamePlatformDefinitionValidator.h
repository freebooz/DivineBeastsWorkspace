#pragma once
#include "EditorValidatorBase.h"
#include "GamePlatformDefinitionValidator.generated.h"

/** 源资产验证器：编辑器及DataValidation命令行入口调用，错误返回Invalid并进入验证诊断。 */
UCLASS()
class UGamePlatformDefinitionValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    /** 所有平台主资产及其派生类都必须验证，非定义子类明确失败。 */
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    /** 独立源扫描重复身份，并对真实对象图检查类型、版本、缺失、环及安全上限。 */
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
