// 项目客户端外观合同：纯字段检查与已加载资源检查；不加载软资产、不拥有权威状态或资源租约。
#include "Characters/DivineBeastsCharacterAppearanceProfile.h"

#include "Animation/Skeleton.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"

#include "Identity/DivineBeastsProjectCatalog.h"

bool UDivineBeastsCharacterAppearanceProfile::ValidateLoadedSkeleton(
    const USkeletalMesh* Mesh, const USkeleton* Skeleton, FString& OutError)
{
    OutError.Reset();
    if (!Mesh || !Skeleton)
    {
        OutError = TEXT("角色网格或骨架未加载。");
        return false;
    }
    const FReferenceSkeleton& MeshBones = Mesh->GetRefSkeleton();
    const FReferenceSkeleton& SkeletonBones = Skeleton->GetReferenceSkeleton();
    if (MeshBones.GetNum() == 0)
    {
        OutError = TEXT("角色网格没有真实骨骼。");
        return false;
    }
    // 引擎IsCompatibleMesh允许缺失子骨骼时沿祖先寻找匹配；项目蒙皮要求完整覆盖及最近有效父链，不能只靠宽松匹配放行。
    for (int32 Index = 0; Index < MeshBones.GetNum(); ++Index)
    {
        const FName Bone = MeshBones.GetBoneName(Index);
        const int32 SkeletonIndex = SkeletonBones.FindBoneIndex(Bone);
        if (SkeletonIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("骨架缺少蒙皮需要的骨骼：%s。"), *Bone.ToString());
            return false;
        }
        int32 SkeletonParent = SkeletonBones.GetParentIndex(SkeletonIndex);
        // 简化网格可以省略骨架中的中间骨骼，但不能把胸部/手部等现有骨骼接到不同父链。
        while (SkeletonParent != INDEX_NONE && MeshBones.FindBoneIndex(SkeletonBones.GetBoneName(SkeletonParent)) == INDEX_NONE)
        {
            SkeletonParent = SkeletonBones.GetParentIndex(SkeletonParent);
        }
        const int32 MeshParent = MeshBones.GetParentIndex(Index);
        const FName ExpectedParent = MeshParent == INDEX_NONE ? NAME_None : MeshBones.GetBoneName(MeshParent);
        const FName ActualParent = SkeletonParent == INDEX_NONE ? NAME_None : SkeletonBones.GetBoneName(SkeletonParent);
        if (ActualParent != ExpectedParent)
        {
            OutError = FString::Printf(TEXT("骨骼父链不匹配：%s，网格父骨骼=%s，骨架父骨骼=%s。"),
                *Bone.ToString(), *ExpectedParent.ToString(), *ActualParent.ToString());
            return false;
        }
    }
    return true;
}

bool UDivineBeastsCharacterAppearanceProfile::ValidateLoadedAnimationClass(
    const USkeletalMesh* Mesh, const UClass* AnimClass, FString& OutError)
{
    // 先验证网格自己的真实骨树；模板或原生动画都不能给缺失Mesh/Skeleton制造成功。
    if (!ValidateLoadedSkeleton(Mesh, Mesh ? Mesh->GetSkeleton() : nullptr, OutError)) return false;
    if (!AnimClass) return true;
    if (!AnimClass->IsChildOf(UAnimInstance::StaticClass()))
    {
        OutError = TEXT("角色动画类必须派生UAnimInstance。");
        return false;
    }
    const IAnimClassInterface* AnimationInterface = IAnimClassInterface::GetFromClass(AnimClass);
    if (!AnimationInterface) return true; // 原生动画没有蓝图目标骨架，保留其自身动画合同；网格门禁已经完成。
    const USkeleton* AnimationSkeleton = AnimationInterface->GetTargetSkeleton();
    // UE5.8 NeedToSpawnAnimScriptInstance对合法模板的空目标使用Mesh Skeleton；不等同于资源缺失。
    if (!AnimationSkeleton) AnimationSkeleton = Mesh->GetSkeleton();
    return ValidateLoadedSkeleton(Mesh, AnimationSkeleton, OutError);
}

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
    if (bDevelopmentPlaceholder)
    {
        if (!FMath::IsFinite(DevelopmentTint.R) ||
            !FMath::IsFinite(DevelopmentTint.G) ||
            !FMath::IsFinite(DevelopmentTint.B) ||
            !FMath::IsFinite(DevelopmentTint.A))
        {
            OutError = TEXT("开发占位Character Appearance颜色必须为有限数值。");
            return false;
        }
    }
    return true;
}
