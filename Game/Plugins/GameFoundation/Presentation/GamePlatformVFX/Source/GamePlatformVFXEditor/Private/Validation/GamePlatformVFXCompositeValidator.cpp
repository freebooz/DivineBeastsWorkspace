#include "Validation/GamePlatformVFXCompositeValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"

bool UGamePlatformVFXCompositeValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject, FDataValidationContext&) const
{
    return InObject && InObject->IsA<UGamePlatformVFXCompositeDefinition>();
}

EDataValidationResult UGamePlatformVFXCompositeValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* InAsset, FDataValidationContext&)
{
    const UGamePlatformVFXCompositeDefinition* Definition = CastChecked<UGamePlatformVFXCompositeDefinition>(InAsset);
    bool bValid = true;

    for (int32 Index = 0; Index < Definition->Children.Num(); ++Index)
    {
        if (!Definition->Children[Index].DefinitionId.IsValid())
        {
            AssetFails(InAsset, FText::FromString(FString::Printf(TEXT("Composite Child[%d] DefinitionId 无效。"), Index)));
            bValid = false;
        }
        if (Definition->Children[Index].DefinitionId == Definition->GetPrimaryAssetId())
        {
            AssetFails(InAsset, FText::FromString(TEXT("Composite 不允许直接引用自身。更深层循环应由项目级图验证继续检查。")));
            bValid = false;
        }
    }

    if (bValid)
    {
        AssetPasses(InAsset);
        return EDataValidationResult::Valid;
    }
    return EDataValidationResult::Invalid;
}
