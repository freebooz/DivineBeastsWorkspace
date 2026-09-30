#include "Definitions/DivineBeastsHeroDefinition.h"

#include "Catalog/DivineBeastsHeroCatalog.h"

namespace
{
    bool ValidateOption(
        const TMap<FString, FString>& Selection,
        const TCHAR* Key,
        const TArray<FName>& Allowed,
        FString& OutError)
    {
        const FString* Value = Selection.Find(Key);
        if (!Value || Value->IsEmpty())
        {
            return true;
        }
        const FName ValueName(**Value);
        if (!Allowed.Contains(ValueName))
        {
            OutError = FString::Printf(
                TEXT("%s=%s不在Definition允许的外观选项中。"),
                Key,
                **Value);
            return false;
        }
        return true;
    }
}

bool FDivineBeastsAppearanceOptionSchema::ValidateSelection(
    const TMap<FString, FString>& Selection,
    FString& OutError) const
{
    static const TSet<FString> KnownKeys =
    {
        TEXT("BodyVariant"),
        TEXT("HeadPreset"),
        TEXT("SkinMarkingPreset")
    };

    for (const TPair<FString, FString>& Pair : Selection)
    {
        if (!KnownKeys.Contains(Pair.Key))
        {
            OutError = TEXT("出现未声明的Appearance字段：") + Pair.Key;
            return false;
        }
    }

    return ValidateOption(Selection, TEXT("BodyVariant"), BodyVariants, OutError)
        && ValidateOption(Selection, TEXT("HeadPreset"), HeadPresets, OutError)
        && ValidateOption(Selection, TEXT("SkinMarkingPreset"), SkinMarkingPresets, OutError);
}

FPrimaryAssetId UDivineBeastsHeroDefinition::GetPrimaryAssetId() const
{
    return DefinitionId.IsNone()
        ? FPrimaryAssetId()
        : FPrimaryAssetId(FPrimaryAssetType(TEXT("DivineBeastsHeroDefinition")), DefinitionId);
}

bool UDivineBeastsHeroDefinition::IsProjectDefinitionValid(FString& OutError) const
{
    if (!IsDefinitionValid(OutError))
    {
        return false;
    }

    EDivineBeastsZodiacIdentity ExpectedZodiac =
        EDivineBeastsZodiacIdentity::Rat;
    if (!FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            DefinitionId,
            ExpectedZodiac) ||
        ExpectedZodiac != ZodiacIdentity)
    {
        OutError = TEXT("DefinitionId与ZodiacIdentity不匹配。");
        return false;
    }

    const FGameplayTag ExpectedTag =
        FDivineBeastsHeroCatalog::GetZodiacTag(ZodiacIdentity);
    if (!ZodiacTag.IsValid() || ZodiacTag != ExpectedTag)
    {
        OutError = TEXT("ZodiacTag与ZodiacIdentity不匹配。");
        return false;
    }

    if (!Momentum.IsValid(OutError))
    {
        return false;
    }

    if (DisplayNameKey.IsNone() || ContentPackId.IsNone())
    {
        OutError = TEXT("DisplayNameKey/ContentPackId不能为空。");
        return false;
    }
    return true;
}
