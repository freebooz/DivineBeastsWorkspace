#include "Context/DivineBeastsPresentationContext.h"

#include "Identity/DivineBeastsProjectCatalog.h"

bool FDivineBeastsPresentationProjectContext::IsValid(FString& OutError) const
{
    if (ProjectId != FDivineBeastsProjectCatalog::GetProjectId())
    {
        OutError = TEXT("ProjectId必须匹配DivineBeastsRuntime项目身份。");
        return false;
    }
    if (WorldGeneration < 0 || AvatarGeneration < 0)
    {
        OutError = TEXT("WorldGeneration/AvatarGeneration不能为负数。");
        return false;
    }
    return true;
}

FGamePlatformPresentationContext
FDivineBeastsPresentationProjectContext::ToPlatformContext() const
{
    FGamePlatformPresentationContext Result;
    Result.ProjectId = ProjectId;
    Result.HeroDefinitionId = HeroDefinitionId;
    Result.AbilityId = AbilityId;
    Result.SkinId = SkinId;
    Result.WorldId = WorldId;
    Result.ExperienceId = ExperienceId;
    Result.RegionId = RegionId;
    Result.ArenaModeId = ArenaModeId;
    Result.ContentPackId = ContentPackId;
    Result.PlatformId = PlatformId;
    Result.WorldGeneration = WorldGeneration;
    Result.AvatarGeneration = AvatarGeneration;
    Result.LocalPlayerRelation = LocalPlayerRelation;
    Result.QualityTier = QualityTier;
    return Result;
}
