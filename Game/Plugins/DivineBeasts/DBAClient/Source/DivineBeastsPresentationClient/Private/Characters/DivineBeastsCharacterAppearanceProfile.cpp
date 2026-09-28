#include "Characters/DivineBeastsCharacterAppearanceProfile.h"

#include "Identity/DivineBeastsProjectCatalog.h"

bool UDivineBeastsCharacterAppearanceProfile::IsProfileValid(
    FString& OutError) const
{
    if (ProfileId.IsNone())
    {
        OutError = TEXT("Character Appearance ProfileId不能为空。");
        return false;
    }
    if (!FDivineBeastsProjectCatalog::IsHeroDefinitionId(HeroDefinitionId))
    {
        OutError = TEXT("Character Appearance的HeroDefinitionId不属于Shared十二生肖目录。");
        return false;
    }
    if (SkeletonCompatibilityId.IsNone())
    {
        OutError = TEXT("Character Appearance的SkeletonCompatibilityId不能为空。");
        return false;
    }
    if (SkeletalMesh.IsNull())
    {
        OutError = TEXT("Character Appearance必须提供SkeletalMesh软引用。");
        return false;
    }
    if (Version <= 0 || ContentRevision.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("Character Appearance版本和ContentRevision必须有效。");
        return false;
    }
    if (MeshRelativeLocation.ContainsNaN() ||
        MeshRelativeRotation.ContainsNaN() ||
        MeshRelativeScale.ContainsNaN())
    {
        OutError = TEXT("Character Appearance的Mesh相对变换必须为有限数值。");
        return false;
    }
    if (MeshRelativeScale.X <= 0.0 ||
        MeshRelativeScale.Y <= 0.0 ||
        MeshRelativeScale.Z <= 0.0)
    {
        OutError = TEXT("Character Appearance的Mesh旋转必须有效且缩放必须为正数。");
        return false;
    }
    for (const TSoftObjectPtr<UMaterialInterface>& Material : MaterialOverrides)
    {
        if (Material.IsNull())
        {
            OutError = TEXT("Character Appearance材质覆盖不能包含空软引用。");
            return false;
        }
    }
    return true;
}
