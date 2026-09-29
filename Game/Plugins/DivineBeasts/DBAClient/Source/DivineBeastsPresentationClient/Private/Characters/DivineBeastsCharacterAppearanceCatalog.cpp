#include "Characters/DivineBeastsCharacterAppearanceCatalog.h"

#include "Characters/DivineBeastsCharacterAppearanceProfile.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

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
#if !UE_BUILD_SHIPPING
    FLinearColor GetDevelopmentTint(const FString& Suffix)
    {
        // 12种高区分度占位色只服务开发识别，不承担五行、阵营或玩法语义。
        if (Suffix == TEXT("Rat"))     return FLinearColor(0.3765f, 0.4902f, 0.5451f, 1.0f); // #607D8B
        if (Suffix == TEXT("Ox"))      return FLinearColor(0.4745f, 0.3333f, 0.2824f, 1.0f); // #795548
        if (Suffix == TEXT("Tiger"))   return FLinearColor(1.0000f, 0.5490f, 0.0000f, 1.0f); // #FF8C00
        if (Suffix == TEXT("Rabbit"))  return FLinearColor(0.9569f, 0.5608f, 0.6941f, 1.0f); // #F48FB1
        if (Suffix == TEXT("Dragon"))  return FLinearColor(0.0000f, 0.6745f, 0.7569f, 1.0f); // #00ACC1
        if (Suffix == TEXT("Snake"))   return FLinearColor(0.1804f, 0.4902f, 0.1961f, 1.0f); // #2E7D32
        if (Suffix == TEXT("Horse"))   return FLinearColor(0.8275f, 0.1843f, 0.1843f, 1.0f); // #D32F2F
        if (Suffix == TEXT("Goat"))    return FLinearColor(0.8314f, 0.6863f, 0.2157f, 1.0f); // #D4AF37
        if (Suffix == TEXT("Monkey"))  return FLinearColor(0.4941f, 0.3412f, 0.7608f, 1.0f); // #7E57C2
        if (Suffix == TEXT("Rooster")) return FLinearColor(0.9922f, 0.8471f, 0.2078f, 1.0f); // #FDD835
        if (Suffix == TEXT("Dog"))     return FLinearColor(0.2235f, 0.2863f, 0.6706f, 1.0f); // #3949AB
        if (Suffix == TEXT("Boar"))    return FLinearColor(0.6784f, 0.0784f, 0.3412f, 1.0f); // #AD1457
        return FLinearColor::White;
    }

    bool UsesMannyPrototype(const FString& Suffix)
    {
        // Manny/Quinn交替分配，让原型阶段除颜色外还有基础体型轮廓差异。
        return Suffix == TEXT("Rat") ||
            Suffix == TEXT("Tiger") ||
            Suffix == TEXT("Dragon") ||
            Suffix == TEXT("Horse") ||
            Suffix == TEXT("Monkey") ||
            Suffix == TEXT("Dog");
    }

    UDivineBeastsCharacterAppearanceProfile* CreateDevelopmentFallbackProfile(
        FName HeroDefinitionId)
    {
        const FString Suffix = GetHeroSuffix(HeroDefinitionId);
        if (Suffix.IsEmpty())
        {
            return nullptr;
        }

        UDivineBeastsCharacterAppearanceProfile* Profile =
            NewObject<UDivineBeastsCharacterAppearanceProfile>(GetTransientPackage());
        if (!Profile)
        {
            return nullptr;
        }

        const bool bManny = UsesMannyPrototype(Suffix);
        Profile->ProfileId =
            FDivineBeastsCharacterAppearanceCatalog::GetDefaultProfileId(HeroDefinitionId);
        Profile->HeroDefinitionId = HeroDefinitionId;
        Profile->SkeletonCompatibilityId = TEXT("Skeleton.UE5.Mannequin");
        Profile->Version = 1;
        Profile->ContentRevision = TEXT("Prototype.Mannequin.1");
        Profile->bDevelopmentPlaceholder = true;
        Profile->DevelopmentTint = GetDevelopmentTint(Suffix);

        const TCHAR* MeshPath = bManny
            ? TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")
            : TEXT("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
        Profile->SkeletalMesh =
            TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));

        // 回退Profile直接保留Mannequin网格自带材质，再通过MID修改Paint Tint；
        // 正式生成器会在每个DBAHeroPack_*内创建独立PrototypeTint材质实例。
        Profile->MaterialOverrides.Reset();

        return Profile;
    }
#endif
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

#if !UE_BUILD_SHIPPING
    // 开发期真实DBAHeroPack_* Profile尚未制作时，直接使用稳定的Mannequin占位Profile。
    // Shipping不允许该分支，从而保证正式包必须交付真实Profile资产。
    const FString LongPackageName = AssetPath.GetLongPackageName();
    if (LongPackageName.IsEmpty() || !FPackageName::DoesPackageExist(LongPackageName))
    {
        if (Completion)
        {
            Completion(CreateDevelopmentFallbackProfile(HeroDefinitionId));
        }
        return nullptr;
    }
#endif

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
            [AssetPath, HeroDefinitionId, Completion = MoveTemp(Completion)]() mutable
            {
                if (!Completion)
                {
                    return;
                }

                UDivineBeastsCharacterAppearanceProfile* Loaded =
                    Cast<UDivineBeastsCharacterAppearanceProfile>(
                        AssetPath.ResolveObject());
#if !UE_BUILD_SHIPPING
                if (!Loaded)
                {
                    Loaded = CreateDevelopmentFallbackProfile(HeroDefinitionId);
                }
#endif
                Completion(Loaded);
            }));
}
