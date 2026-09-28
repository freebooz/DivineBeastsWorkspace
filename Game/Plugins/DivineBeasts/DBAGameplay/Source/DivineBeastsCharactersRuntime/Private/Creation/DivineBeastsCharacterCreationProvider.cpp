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
            const bool bHasRegisteredDefinition =
                FDivineBeastsHeroCatalog::GetDefinitionAssetPath(
                    Entry.HeroDefinitionId).IsValid();
#if UE_BUILD_SHIPPING
            // Shipping必须存在真实、已注册并可Cook的Definition资产；不允许使用开发占位回退。
            if (!bHasRegisteredDefinition)
            {
                continue;
            }
#endif
            FGamePlatformCharacterCreationHeroDescriptor Descriptor;
            Descriptor.HeroDefinitionId = Entry.HeroDefinitionId;
            Descriptor.DisplayNameKey = Entry.DisplayNameKey;
            Descriptor.ContentRevision = bHasRegisteredDefinition
                ? FString::Printf(
                    TEXT("Catalog.%d"),
                    FDivineBeastsHeroCatalog::CatalogRevision)
                : FDivineBeastsHeroCatalog::GetDevelopmentFallbackContentRevision();
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
#if UE_BUILD_SHIPPING
            OutError = TEXT("Hero Definition资产尚未注册到AssetManager，Shipping禁止使用占位回退。");
            return false;
#else
            // 开发阶段真实Definition尚未生成时，只允许默认空外观草稿；颜色和Mannequin外观由内容包生成器决定。
            if (!AppearanceSelection.IsEmpty())
            {
                OutError = TEXT("开发占位Definition只允许默认外观；请先生成真实Hero Definition资产再提交自定义外观。");
                return false;
            }
            return true;
#endif
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
