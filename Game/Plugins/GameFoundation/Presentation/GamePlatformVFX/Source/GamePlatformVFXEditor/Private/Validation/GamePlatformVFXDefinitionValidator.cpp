#include "Validation/GamePlatformVFXDefinitionValidator.h"
#include "Definitions/GamePlatformVFXDefinition.h"

bool UGamePlatformVFXDefinitionValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject, FDataValidationContext&) const
{
    return InObject && InObject->IsA<UGamePlatformVFXDefinition>();
}

EDataValidationResult UGamePlatformVFXDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* InAsset, FDataValidationContext&)
{
    const UGamePlatformVFXDefinition* Definition = CastChecked<UGamePlatformVFXDefinition>(InAsset);
    bool bValid = true;

    if (Definition->StableId.IsNone())
    {
        AssetFails(InAsset, FText::FromString(TEXT("VFX Definition 必须配置 StableId。")));
        bValid = false;
    }

    if (Definition->SchemaVersion < 1)
    {
        AssetFails(InAsset, FText::FromString(TEXT("SchemaVersion 必须 >= 1。")));
        bValid = false;
    }

    if (Definition->Behavior != EGamePlatformVFXBehavior::Composite && Definition->NiagaraSystem.IsNull())
    {
        AssetFails(InAsset, FText::FromString(TEXT("非 Composite VFX Definition 必须配置 NiagaraSystem。")));
        bValid = false;
    }

    if (bValid)
    {
        AssetPasses(InAsset);
        return EDataValidationResult::Valid;
    }

    return EDataValidationResult::Invalid;
}
