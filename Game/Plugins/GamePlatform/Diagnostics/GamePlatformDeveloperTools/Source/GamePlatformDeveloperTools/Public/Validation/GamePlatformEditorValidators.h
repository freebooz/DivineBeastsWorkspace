#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GamePlatformEditorValidators.generated.h"

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
protected:
    virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

/** UGamePlatformStableIdValidator（平台稳定ID验证器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformStableIdValidator : public UEditorValidatorBase
{
    GENERATED_BODY()
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
