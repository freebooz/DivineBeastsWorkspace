#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GamePlatformEditorValidators.generated.h"
// 平台Editor DataValidation注册入口：游戏线程按资产验证，结果写入引擎验证上下文，不保存Runtime业务/资源所有权。
// 实现参数为引擎提供的元数据、已加载只读对象和单次上下文；Invalid/Valid分别表示本规则失败/通过，非Cook或运行完成。

/** UGamePlatformNamingValidator（游戏平台命名验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformNamingValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformContentValidator（游戏平台内容验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformContentValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformDefinitionValidator（平台Definition验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformDefinitionValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
    friend class FGamePlatformDefinitionIdentityAuditTest;
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformStableIdValidator（平台稳定ID验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformStableIdValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
    friend class FGamePlatformDefinitionIdentityAuditTest;
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformGameplayTagValidator（玩法标签验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformGameplayTagValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformServerAssetSafetyValidator（服务器资产安全验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformServerAssetSafetyValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
