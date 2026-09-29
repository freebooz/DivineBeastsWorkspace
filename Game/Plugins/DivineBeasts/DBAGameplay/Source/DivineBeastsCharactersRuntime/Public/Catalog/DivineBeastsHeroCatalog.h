#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Identity/DivineBeastsZodiacIdentity.h"

struct FStreamableHandle;
class UDivineBeastsHeroDefinition;

/** FDivineBeastsCoreHeroCatalogEntry（神兽联盟核心生肖英雄目录项）。 */
struct DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsCoreHeroCatalogEntry
{
    FName HeroDefinitionId = NAME_None;
    EDivineBeastsZodiacIdentity ZodiacIdentity = EDivineBeastsZodiacIdentity::Rat;
    FName DisplayNameKey = NAME_None;
    FName ExpectedAssetName = NAME_None;
};

/**
 * FDivineBeastsHeroCatalog（神兽联盟英雄目录）。
 * Stable ID/生肖/本地化Key在源码中稳定；Definition数据本体仍由UE Primary Asset提供。
 */
class DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsHeroCatalog
{
public:
    static constexpr int32 CatalogRevision = 1;

    static const TArray<FDivineBeastsCoreHeroCatalogEntry>& GetCoreEntries();
    static const TArray<FName>& GetCoreHeroIds();

    static bool IsCoreHeroId(FName HeroDefinitionId);
    static bool TryGetZodiacIdentity(
        FName HeroDefinitionId,
        EDivineBeastsZodiacIdentity& OutZodiacIdentity);

    static FGameplayTag GetZodiacTag(EDivineBeastsZodiacIdentity ZodiacIdentity);
    static FName GetDisplayNameKey(FName HeroDefinitionId);
    static FName GetExpectedDefinitionAssetName(FName HeroDefinitionId);
    static FName GetCoreContentPackId();
    /** 返回单个生肖稳定内容包逻辑ID，例如ContentPack.Hero.Zodiac.Rat；后期真实角色继续沿用。 */
    static FName GetHeroContentPackId(FName HeroDefinitionId);

    /** 返回稳定默认外观Profile ID，例如Appearance.Hero.Zodiac.Rat.Default；占位和正式美术共用同一逻辑身份。 */
    static FName GetDefaultAppearanceProfileId(FName HeroDefinitionId);

    /** 非Shipping开发回退Definition的显式内容修订号；正式资产不得复用该修订号。 */
    static FString GetDevelopmentFallbackContentRevision();

    static FPrimaryAssetId GetDefinitionPrimaryAssetId(FName HeroDefinitionId);
    static FSoftObjectPath GetDefinitionAssetPath(FName HeroDefinitionId);

    static TSharedPtr<FStreamableHandle> RequestDefinition(
        FName HeroDefinitionId,
        TFunction<void(UDivineBeastsHeroDefinition*)> Completion);
};
