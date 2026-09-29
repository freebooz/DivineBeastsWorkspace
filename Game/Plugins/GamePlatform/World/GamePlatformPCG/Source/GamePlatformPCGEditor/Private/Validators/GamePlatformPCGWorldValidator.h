#pragma once

#include "EditorValidatorBase.h"
#include "GamePlatformPCGWorldValidator.generated.h"

/**
 * UGamePlatformPCGWorldValidator（PCG世界编辑器校验器）。
 * 只对真实UWorld资产工作：无PCG内容时不适用；有PCG放置器时要求唯一WorldDirector并完整显式注册。
 */
UCLASS()
class UGamePlatformPCGWorldValidator final : public UEditorValidatorBase
{
    GENERATED_BODY()

protected:
    virtual bool CanValidateAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InObject,
        FDataValidationContext& InContext) const override;

    virtual EDataValidationResult ValidateLoadedAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InAsset,
        FDataValidationContext& Context) override;
};
