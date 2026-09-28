#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"

#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Loading/GamePlatformAssetLoader.h"

namespace
{
    FString GetHeroSuffix(FName HeroDefinitionId)
    {
        if (!FDivineBeastsProjectCatalog::IsHeroDefinitionId(HeroDefinitionId))
        {
            return FString();
        }
        constexpr const TCHAR* Prefix = TEXT("Hero.Zodiac.");
        return HeroDefinitionId.ToString().RightChop(FCString::Strlen(Prefix));
    }
}

FName FDivineBeastsCharacterAppearanceCatalog::GetDefaultProfileId(
    FName HeroDefinitionId)
{
    const FString Suffix = GetHeroSuffix(HeroDefinitionId);
    return Suffix.IsEmpty()
        ? NAME_None
        : FName(*(FString(TEXT("Appearance.Hero.Zodiac.")) + Suffix + TEXT(".Default")));
}

FSoftObjectPath FDivineBeastsCharacterAppearanceCatalog::GetDefaultProfileAssetPath(
    FName HeroDefinitionId)
{
    const FString Suffix = GetHeroSuffix(HeroDefinitionId);
    if (Suffix.IsEmpty())
    {
        return FSoftObjectPath();
    }

    // Profile路径属于项目表现资产约定，不属于跨语言协议；Boar等名称直接来自Shared Hero ID后缀。
    const FString AssetName = FString(TEXT("DA_Appearance_Zodiac_")) + Suffix;
    const FString PackagePath =
        FString(TEXT("/DBAHeroPack_")) + Suffix + TEXT("/Characters/") + AssetName;
    return FSoftObjectPath(PackagePath + TEXT(".") + AssetName);
}

TSharedPtr<FStreamableHandle>
FDivineBeastsCharacterAppearanceCatalog::RequestDefaultProfile(
    FName HeroDefinitionId,
    TFunction<void(UDivineBeastsCharacterAppearanceProfile*)> Completion)
{
    const FSoftObjectPath AssetPath = GetDefaultProfileAssetPath(HeroDefinitionId);
    if (!AssetPath.IsValid())
    {
        if (Completion)
        {
            Completion(nullptr);
        }
        return nullptr;
    }

    if (UDivineBeastsCharacterAppearanceProfile* Existing =
            Cast<UDivineBeastsCharacterAppearanceProfile>(AssetPath.ResolveObject()))
    {
        if (Completion)
        {
            Completion(Existing);
        }
        return nullptr;
    }

    return FGamePlatformAssetLoader::RequestAsyncLoad(
        { AssetPath },
        FStreamableDelegate::CreateLambda(
            [AssetPath, Completion = MoveTemp(Completion)]() mutable
            {
                if (Completion)
                {
                    Completion(
                        Cast<UDivineBeastsCharacterAppearanceProfile>(
                            AssetPath.ResolveObject()));
                }
            }));
}
