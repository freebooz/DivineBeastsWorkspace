#include "Catalog/DivineBeastsHeroCatalog.h"

#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Loading/GamePlatformHeroDefinitionLoader.h"
#include "Tags/DivineBeastsCharacterTags.h"

namespace
{
    bool TryBuildEntry(
        FName HeroDefinitionId,
        FDivineBeastsCoreHeroCatalogEntry& OutEntry)
    {
        struct FLocalZodiacMetadata
        {
            const TCHAR* Suffix;
            EDivineBeastsZodiacIdentity Zodiac;
            const TCHAR* AssetName;
        };

        // Shared只拥有跨语言稳定HeroDefinitionId；UE本地仍可维护不跨协议的生肖枚举与资产命名元数据。
        static const FLocalZodiacMetadata Metadata[] =
        {
            { TEXT(".Rat"), EDivineBeastsZodiacIdentity::Rat, TEXT("DA_Hero_Zodiac_Rat") },
            { TEXT(".Ox"), EDivineBeastsZodiacIdentity::Ox, TEXT("DA_Hero_Zodiac_Ox") },
            { TEXT(".Tiger"), EDivineBeastsZodiacIdentity::Tiger, TEXT("DA_Hero_Zodiac_Tiger") },
            { TEXT(".Rabbit"), EDivineBeastsZodiacIdentity::Rabbit, TEXT("DA_Hero_Zodiac_Rabbit") },
            { TEXT(".Dragon"), EDivineBeastsZodiacIdentity::Dragon, TEXT("DA_Hero_Zodiac_Dragon") },
            { TEXT(".Snake"), EDivineBeastsZodiacIdentity::Snake, TEXT("DA_Hero_Zodiac_Snake") },
            { TEXT(".Horse"), EDivineBeastsZodiacIdentity::Horse, TEXT("DA_Hero_Zodiac_Horse") },
            { TEXT(".Goat"), EDivineBeastsZodiacIdentity::Goat, TEXT("DA_Hero_Zodiac_Goat") },
            { TEXT(".Monkey"), EDivineBeastsZodiacIdentity::Monkey, TEXT("DA_Hero_Zodiac_Monkey") },
            { TEXT(".Rooster"), EDivineBeastsZodiacIdentity::Rooster, TEXT("DA_Hero_Zodiac_Rooster") },
            { TEXT(".Dog"), EDivineBeastsZodiacIdentity::Dog, TEXT("DA_Hero_Zodiac_Dog") },
            { TEXT(".Boar"), EDivineBeastsZodiacIdentity::Boar, TEXT("DA_Hero_Zodiac_Boar") }
        };

        const FString HeroIdString = HeroDefinitionId.ToString();
        for (const FLocalZodiacMetadata& Item : Metadata)
        {
            if (HeroIdString.EndsWith(Item.Suffix, ESearchCase::CaseSensitive))
            {
                OutEntry.HeroDefinitionId = HeroDefinitionId;
                OutEntry.ZodiacIdentity = Item.Zodiac;
                OutEntry.DisplayNameKey = FName(*(HeroIdString + TEXT(".Name")));
                OutEntry.ExpectedAssetName = FName(Item.AssetName);
                return true;
            }
        }
        return false;
    }

    const TArray<FDivineBeastsCoreHeroCatalogEntry>& Entries()
    {
        static const TArray<FDivineBeastsCoreHeroCatalogEntry> Value = []
        {
            TArray<FDivineBeastsCoreHeroCatalogEntry> Result;
            const TArray<FName>& SharedHeroIds =
                FDivineBeastsProjectCatalog::GetHeroDefinitionIds();
            Result.Reserve(SharedHeroIds.Num());
            for (const FName HeroId : SharedHeroIds)
            {
                FDivineBeastsCoreHeroCatalogEntry Entry;
                if (TryBuildEntry(HeroId, Entry))
                {
                    Result.Add(MoveTemp(Entry));
                }
            }
            return Result;
        }();
        return Value;
    }

    const FDivineBeastsCoreHeroCatalogEntry* Find(FName HeroDefinitionId)
    {
        return Entries().FindByPredicate(
            [HeroDefinitionId](const FDivineBeastsCoreHeroCatalogEntry& Entry)
            {
                return Entry.HeroDefinitionId == HeroDefinitionId;
            });
    }
}

const TArray<FDivineBeastsCoreHeroCatalogEntry>&
FDivineBeastsHeroCatalog::GetCoreEntries()
{
    return Entries();
}

const TArray<FName>& FDivineBeastsHeroCatalog::GetCoreHeroIds()
{
    static const TArray<FName> Ids = []
    {
        TArray<FName> Result;
        Result.Reserve(Entries().Num());
        for (const FDivineBeastsCoreHeroCatalogEntry& Entry : Entries())
        {
            Result.Add(Entry.HeroDefinitionId);
        }
        return Result;
    }();
    return Ids;
}

bool FDivineBeastsHeroCatalog::IsCoreHeroId(FName HeroDefinitionId)
{
    return FDivineBeastsProjectCatalog::IsHeroDefinitionId(HeroDefinitionId)
        && Find(HeroDefinitionId) != nullptr;
}

bool FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
    FName HeroDefinitionId,
    EDivineBeastsZodiacIdentity& OutZodiacIdentity)
{
    const FDivineBeastsCoreHeroCatalogEntry* Entry = Find(HeroDefinitionId);
    if (!Entry)
    {
        return false;
    }
    OutZodiacIdentity = Entry->ZodiacIdentity;
    return true;
}

FGameplayTag FDivineBeastsHeroCatalog::GetZodiacTag(
    EDivineBeastsZodiacIdentity ZodiacIdentity)
{
    using namespace DivineBeastsCharacterTags;
    switch (ZodiacIdentity)
    {
    case EDivineBeastsZodiacIdentity::Rat: return Zodiac_Rat;
    case EDivineBeastsZodiacIdentity::Ox: return Zodiac_Ox;
    case EDivineBeastsZodiacIdentity::Tiger: return Zodiac_Tiger;
    case EDivineBeastsZodiacIdentity::Rabbit: return Zodiac_Rabbit;
    case EDivineBeastsZodiacIdentity::Dragon: return Zodiac_Dragon;
    case EDivineBeastsZodiacIdentity::Snake: return Zodiac_Snake;
    case EDivineBeastsZodiacIdentity::Horse: return Zodiac_Horse;
    case EDivineBeastsZodiacIdentity::Goat: return Zodiac_Goat;
    case EDivineBeastsZodiacIdentity::Monkey: return Zodiac_Monkey;
    case EDivineBeastsZodiacIdentity::Rooster: return Zodiac_Rooster;
    case EDivineBeastsZodiacIdentity::Dog: return Zodiac_Dog;
    case EDivineBeastsZodiacIdentity::Boar: return Zodiac_Boar;
    default: return FGameplayTag();
    }
}

FName FDivineBeastsHeroCatalog::GetDisplayNameKey(FName HeroDefinitionId)
{
    const FDivineBeastsCoreHeroCatalogEntry* Entry = Find(HeroDefinitionId);
    return Entry ? Entry->DisplayNameKey : NAME_None;
}

FName FDivineBeastsHeroCatalog::GetExpectedDefinitionAssetName(FName HeroDefinitionId)
{
    const FDivineBeastsCoreHeroCatalogEntry* Entry = Find(HeroDefinitionId);
    return Entry ? Entry->ExpectedAssetName : NAME_None;
}

FName FDivineBeastsHeroCatalog::GetCoreContentPackId()
{
    return TEXT("ContentPack.Hero.Zodiac.Core");
}

FName FDivineBeastsHeroCatalog::GetHeroContentPackId(FName HeroDefinitionId)
{
    if (!IsCoreHeroId(HeroDefinitionId))
    {
        return NAME_None;
    }
    const FString Suffix = HeroDefinitionId.ToString().RightChop(
        FString(TEXT("Hero.Zodiac.")).Len());
    return FName(*(FString(TEXT("ContentPack.Hero.Zodiac.")) + Suffix));
}

FName FDivineBeastsHeroCatalog::GetDefaultAppearanceProfileId(FName HeroDefinitionId)
{
    if (!IsCoreHeroId(HeroDefinitionId))
    {
        return NAME_None;
    }
    const FString Suffix = HeroDefinitionId.ToString().RightChop(
        FString(TEXT("Hero.Zodiac.")).Len());
    return FName(*(FString(TEXT("Appearance.Hero.Zodiac.")) + Suffix + TEXT(".Default")));
}

FString FDivineBeastsHeroCatalog::GetDevelopmentFallbackContentRevision()
{
    return TEXT("Prototype.Placeholder.1");
}

FPrimaryAssetId FDivineBeastsHeroCatalog::GetDefinitionPrimaryAssetId(
    FName HeroDefinitionId)
{
    return IsCoreHeroId(HeroDefinitionId)
        ? FPrimaryAssetId(FPrimaryAssetType(TEXT("DivineBeastsHeroDefinition")), HeroDefinitionId)
        : FPrimaryAssetId();
}

FSoftObjectPath FDivineBeastsHeroCatalog::GetDefinitionAssetPath(
    FName HeroDefinitionId)
{
    const FPrimaryAssetId PrimaryId =
        GetDefinitionPrimaryAssetId(HeroDefinitionId);
    return PrimaryId.IsValid()
        ? UAssetManager::Get().GetPrimaryAssetPath(PrimaryId)
        : FSoftObjectPath();
}

TSharedPtr<FStreamableHandle> FDivineBeastsHeroCatalog::RequestDefinition(
    FName HeroDefinitionId,
    TFunction<void(UDivineBeastsHeroDefinition*)> Completion)
{
    const FPrimaryAssetId PrimaryAssetId = GetDefinitionPrimaryAssetId(HeroDefinitionId);
    const FSoftObjectPath AssetPath = PrimaryAssetId.IsValid()
        ? UAssetManager::Get().GetPrimaryAssetPath(PrimaryAssetId)
        : FSoftObjectPath();

#if !UE_BUILD_SHIPPING
    if (IsCoreHeroId(HeroDefinitionId) && !AssetPath.IsValid())
    {
        // 开发环境允许在真实DataAsset尚未由Unreal Editor生成时使用纯逻辑回退，
        // 仅用于打通角色选择、Spawn、复制和版本门禁；不包含Mesh、材质、动画、VFX或UI资产。
        UDivineBeastsHeroDefinition* Fallback =
            NewObject<UDivineBeastsHeroDefinition>(GetTransientPackage());
        Fallback->DefinitionId = HeroDefinitionId;
        Fallback->Version = 1;
        Fallback->ContentRevision = GetDevelopmentFallbackContentRevision();
        Fallback->AppearanceProfileId = GetDefaultAppearanceProfileId(HeroDefinitionId);
        Fallback->PresentationProfileId =
            FName(*(FString(TEXT("Presentation.")) + HeroDefinitionId.ToString() + TEXT(".Default")));
        Fallback->SkeletonCompatibilityId = TEXT("Skeleton.UE5.Mannequin");
        Fallback->DisplayNameKey = GetDisplayNameKey(HeroDefinitionId);
        Fallback->ContentPackId = GetHeroContentPackId(HeroDefinitionId);
        TryGetZodiacIdentity(HeroDefinitionId, Fallback->ZodiacIdentity);
        Fallback->ZodiacTag = GetZodiacTag(Fallback->ZodiacIdentity);
        Fallback->AppearanceSchema.BodyVariants = { TEXT("Default") };
        Fallback->AppearanceSchema.HeadPresets = { TEXT("Default") };
        Fallback->AppearanceSchema.SkinMarkingPresets = { TEXT("None") };

        if (Completion)
        {
            Completion(Fallback);
        }
        return nullptr;
    }
#endif

    return FGamePlatformHeroDefinitionLoader::RequestDefinition(
        PrimaryAssetId,
        [Completion = MoveTemp(Completion)](
            UGamePlatformHeroDefinition* Definition) mutable
        {
            if (Completion)
            {
                Completion(Cast<UDivineBeastsHeroDefinition>(Definition));
            }
        });
}
