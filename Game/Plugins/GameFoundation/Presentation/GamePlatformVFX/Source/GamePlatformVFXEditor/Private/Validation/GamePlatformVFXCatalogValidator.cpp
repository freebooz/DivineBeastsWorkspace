#include "Validation/GamePlatformVFXCatalogValidator.h"
#include "Catalogs/GamePlatformVFXCatalog.h"

bool UGamePlatformVFXCatalogValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* InObject, FDataValidationContext&) const
{
    return InObject && InObject->IsA<UGamePlatformVFXCatalog>();
}

EDataValidationResult UGamePlatformVFXCatalogValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* InAsset, FDataValidationContext&)
{
    const UGamePlatformVFXCatalog* Catalog = CastChecked<UGamePlatformVFXCatalog>(InAsset);
    bool bValid = true;

    if (Catalog->StableId.IsNone())
    {
        AssetFails(InAsset, FText::FromString(TEXT("VFX Catalog 必须配置 StableId。")));
        bValid = false;
    }

    TMap<FString, FPrimaryAssetId> DeterministicKeys;
    for (int32 Index = 0; Index < Catalog->Entries.Num(); ++Index)
    {
        const FGamePlatformVFXCatalogEntry& Entry = Catalog->Entries[Index];
        if (!Entry.SemanticTag.IsValid() || !Entry.DefinitionId.IsValid())
        {
            AssetFails(InAsset, FText::FromString(FString::Printf(TEXT("Catalog Entry[%d] 的 SemanticTag 或 DefinitionId 无效。"), Index)));
            bValid = false;
            continue;
        }

        const FString Key = FString::Printf(TEXT("%s|%d|%d|%s|%s"),
            *Entry.SemanticTag.ToString(),
            static_cast<int32>(Entry.Scope),
            Entry.Priority,
            *Entry.RequiredContextTags.ToStringSimple(false),
            *Entry.BlockedContextTags.ToStringSimple(false));

        if (const FPrimaryAssetId* Existing = DeterministicKeys.Find(Key))
        {
            if (*Existing != Entry.DefinitionId)
            {
                AssetFails(InAsset, FText::FromString(FString::Printf(TEXT("Catalog 存在确定性歧义：%s"), *Key)));
                bValid = false;
            }
        }
        else
        {
            DeterministicKeys.Add(Key, Entry.DefinitionId);
        }
    }

    if (bValid)
    {
        AssetPasses(InAsset);
        return EDataValidationResult::Valid;
    }
    return EDataValidationResult::Invalid;
}
