#include "Definitions/DivineBeastsWorldDefinition.h"

#include "Identity/DivineBeastsProjectCatalog.h"

FGamePlatformResult UDivineBeastsWorldDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (!FDivineBeastsProjectCatalog::IsServerRoleId(ServerRoleId))
    {
        return FGamePlatformResult::Failure(
            TEXT("InvalidProjectServerRole"),
            TEXT("项目世界必须指定Shared目录中的正式服务器角色。"));
    }

    if (!DefaultExperienceId.IsValid())
    {
        return FGamePlatformResult::Failure(
            TEXT("ProjectExperienceRequired"),
            TEXT("项目世界必须填写完整的默认体验逻辑身份。"));
    }
    if (DefaultExperienceId.LogicalVersion != 1)
    {
        return FGamePlatformResult::Failure(
            TEXT("UnsupportedProjectExperienceVersion"),
            TEXT("当前Shared体验目录只发布逻辑版本1，不能将未知版本当作兼容体验。"));
    }

    const FString ExperiencePath =
        DefaultExperienceId.Namespace + TEXT(".") + DefaultExperienceId.Name;
    const FName ExperienceId(*ExperiencePath);
    FName MappedRoleId = NAME_None;
    if (!FDivineBeastsProjectCatalog::IsExperienceId(ExperienceId) ||
        !FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
            ExperienceId,
            MappedRoleId) ||
        MappedRoleId != ServerRoleId)
    {
        return FGamePlatformResult::Failure(
            TEXT("ProjectRoleExperienceMismatch"),
            TEXT("项目世界的ServerRoleId必须与Shared目录中DefaultExperienceId映射一致。"));
    }

    if (!ArenaModeId.IsNone())
    {
        FName ModeRoleId = NAME_None;
        FName ModeExperienceId = NAME_None;
        if (!FDivineBeastsProjectCatalog::TryGetArenaContextForMode(
                ArenaModeId,
                ModeRoleId,
                ModeExperienceId))
        {
            return FGamePlatformResult::Failure(
                TEXT("InvalidProjectArenaMode"),
                TEXT("ArenaModeId必须是Shared目录中的正式竞技模式。"));
        }
        if (ModeRoleId != ServerRoleId || ModeExperienceId != ExperienceId)
        {
            return FGamePlatformResult::Failure(
                TEXT("ArenaModeWorldContextMismatch"),
                TEXT("竞技模式映射角色和体验必须与该世界定义一致；非竞技世界不能携带竞技模式。"));
        }
    }

    return FGamePlatformResult::Success();
}
