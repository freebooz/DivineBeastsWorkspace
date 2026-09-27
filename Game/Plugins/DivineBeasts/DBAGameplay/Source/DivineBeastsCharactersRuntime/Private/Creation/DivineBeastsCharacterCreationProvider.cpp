#include "Creation/DivineBeastsCharacterCreationProvider.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/AssetManager.h"

class FDivineBeastsCharacterCreationProvider final
    : public IDivineBeastsCharacterCreationProvider
{
public:
    virtual int32 GetCatalogRevision() const override
    {
        return FDivineBeastsHeroCatalog::CatalogRevision;
    }

    virtual void GetCreateableHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const override
    {
        OutHeroes.Reset();
        for (const FDivineBeastsCoreHeroCatalogEntry& Entry :
             FDivineBeastsHeroCatalog::GetCoreEntries())
        {
            // Core Catalog可以声明12个稳定身份，但只有已注册真实Definition资产的Hero才是当前客户端可创建项。
            if (!FDivineBeastsHeroCatalog::GetDefinitionAssetPath(
                    Entry.HeroDefinitionId).IsValid())
            {
                continue;
            }
            FGamePlatformCharacterCreationHeroDescriptor Descriptor;
            Descriptor.HeroDefinitionId = Entry.HeroDefinitionId;
            Descriptor.DisplayNameKey = Entry.DisplayNameKey;
            Descriptor.ContentRevision = FString::Printf(
                TEXT("Catalog.%d"),
                FDivineBeastsHeroCatalog::CatalogRevision);
            OutHeroes.Add(MoveTemp(Descriptor));
        }
    }

    virtual bool ValidateCreationDraft(
        FName HeroDefinitionId,
        const TMap<FString, FString>& AppearanceSelection,
        FString& OutError) const override
    {
        if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
        {
            OutError = TEXT("HeroDefinitionId不在十二生肖核心Catalog中。");
            return false;
        }

        const FSoftObjectPath DefinitionPath =
            FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroDefinitionId);
        if (!DefinitionPath.IsValid())
        {
            OutError = TEXT("Hero Definition资产尚未注册到AssetManager。");
            return false;
        }

        if (const UDivineBeastsHeroDefinition* Definition =
            Cast<UDivineBeastsHeroDefinition>(DefinitionPath.ResolveObject()))
        {
            return Definition->AppearanceSchema.ValidateSelection(
                AppearanceSelection,
                OutError);
        }

        // Definition尚未载入时只允许空外观选择；非空选择必须等待Definition加载后再验证。
        if (!AppearanceSelection.IsEmpty())
        {
            OutError = TEXT("Hero Definition尚未载入，无法验证AppearanceSelection。");
            return false;
        }
        return true;
    }

    virtual bool TryGetZodiacIdentity(
        FName HeroDefinitionId,
        EDivineBeastsZodiacIdentity& OutZodiacIdentity) const override
    {
        return FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            HeroDefinitionId,
            OutZodiacIdentity);
    }
};

TUniquePtr<IDivineBeastsCharacterCreationProvider>
CreateDivineBeastsCharacterCreationProvider()
{
    return MakeUnique<FDivineBeastsCharacterCreationProvider>();
}
