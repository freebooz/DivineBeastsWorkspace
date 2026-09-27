#include "Catalog/DivineBeastsHeroCatalog.h"

#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Loading/GamePlatformHeroDefinitionLoader.h"
#include "Tags/DivineBeastsCharacterTags.h"

namespace
{
    const TArray<FDivineBeastsCoreHeroCatalogEntry>& Entries()
    {
        static const TArray<FDivineBeastsCoreHeroCatalogEntry> Value =
        {
            { TEXT("Hero.Zodiac.Rat"), EDivineBeastsZodiacIdentity::Rat, TEXT("Hero.Zodiac.Rat.Name"), TEXT("DA_Hero_Zodiac_Rat") },
            { TEXT("Hero.Zodiac.Ox"), EDivineBeastsZodiacIdentity::Ox, TEXT("Hero.Zodiac.Ox.Name"), TEXT("DA_Hero_Zodiac_Ox") },
            { TEXT("Hero.Zodiac.Tiger"), EDivineBeastsZodiacIdentity::Tiger, TEXT("Hero.Zodiac.Tiger.Name"), TEXT("DA_Hero_Zodiac_Tiger") },
            { TEXT("Hero.Zodiac.Rabbit"), EDivineBeastsZodiacIdentity::Rabbit, TEXT("Hero.Zodiac.Rabbit.Name"), TEXT("DA_Hero_Zodiac_Rabbit") },
            { TEXT("Hero.Zodiac.Dragon"), EDivineBeastsZodiacIdentity::Dragon, TEXT("Hero.Zodiac.Dragon.Name"), TEXT("DA_Hero_Zodiac_Dragon") },
            { TEXT("Hero.Zodiac.Snake"), EDivineBeastsZodiacIdentity::Snake, TEXT("Hero.Zodiac.Snake.Name"), TEXT("DA_Hero_Zodiac_Snake") },
            { TEXT("Hero.Zodiac.Horse"), EDivineBeastsZodiacIdentity::Horse, TEXT("Hero.Zodiac.Horse.Name"), TEXT("DA_Hero_Zodiac_Horse") },
            { TEXT("Hero.Zodiac.Goat"), EDivineBeastsZodiacIdentity::Goat, TEXT("Hero.Zodiac.Goat.Name"), TEXT("DA_Hero_Zodiac_Goat") },
            { TEXT("Hero.Zodiac.Monkey"), EDivineBeastsZodiacIdentity::Monkey, TEXT("Hero.Zodiac.Monkey.Name"), TEXT("DA_Hero_Zodiac_Monkey") },
            { TEXT("Hero.Zodiac.Rooster"), EDivineBeastsZodiacIdentity::Rooster, TEXT("Hero.Zodiac.Rooster.Name"), TEXT("DA_Hero_Zodiac_Rooster") },
            { TEXT("Hero.Zodiac.Dog"), EDivineBeastsZodiacIdentity::Dog, TEXT("Hero.Zodiac.Dog.Name"), TEXT("DA_Hero_Zodiac_Dog") },
            { TEXT("Hero.Zodiac.Boar"), EDivineBeastsZodiacIdentity::Boar, TEXT("Hero.Zodiac.Boar.Name"), TEXT("DA_Hero_Zodiac_Boar") }
        };
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
    return Find(HeroDefinitionId) != nullptr;
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
    return FGamePlatformHeroDefinitionLoader::RequestDefinition(
        GetDefinitionPrimaryAssetId(HeroDefinitionId),
        [Completion = MoveTemp(Completion)](
            UGamePlatformHeroDefinition* Definition) mutable
        {
            if (Completion)
            {
                Completion(Cast<UDivineBeastsHeroDefinition>(Definition));
            }
        });
}
