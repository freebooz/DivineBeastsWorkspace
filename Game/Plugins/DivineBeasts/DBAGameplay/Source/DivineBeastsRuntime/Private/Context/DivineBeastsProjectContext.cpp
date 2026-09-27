#include "Context/DivineBeastsProjectContext.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "Version/DivineBeastsContractVersion.h"

void FDivineBeastsProjectContext::InitializeCanonicalIdentity()
{
    GameId = FDivineBeastsProjectCatalog::GetGameId();
    ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    ContractVersion = FDivineBeastsContractVersion::GetCurrentVersion();
}

EDivineBeastsProjectContextError FDivineBeastsProjectContext::Validate() const
{
    if (GameId != FDivineBeastsProjectCatalog::GetGameId())
    {
        return EDivineBeastsProjectContextError::InvalidGameId;
    }
    if (ProjectId != FDivineBeastsProjectCatalog::GetProjectId())
    {
        return EDivineBeastsProjectContextError::InvalidProjectId;
    }
    if (!FDivineBeastsProjectCatalog::IsServerRoleId(ServerRoleId))
    {
        return EDivineBeastsProjectContextError::InvalidServerRole;
    }
    if (!FDivineBeastsProjectCatalog::IsExperienceId(ExperienceId))
    {
        return EDivineBeastsProjectContextError::InvalidExperience;
    }

    FName ExpectedRole = NAME_None;
    if (!FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
            ExperienceId,
            ExpectedRole) ||
        ExpectedRole != ServerRoleId)
    {
        return EDivineBeastsProjectContextError::RoleExperienceMismatch;
    }

    if (!ArenaModeId.IsNone())
    {
        if (!FDivineBeastsProjectCatalog::IsArenaModeId(ArenaModeId))
        {
            return EDivineBeastsProjectContextError::InvalidArenaMode;
        }
        if (ServerRoleId != FDivineBeastsProjectCatalog::GetMainArenaServerRole())
        {
            return EDivineBeastsProjectContextError::ArenaModeRequiresMainArena;
        }

        FName ArenaRole = NAME_None;
        FName ArenaExperience = NAME_None;
        if (!FDivineBeastsProjectCatalog::TryGetArenaContextForMode(
                ArenaModeId,
                ArenaRole,
                ArenaExperience) ||
            ArenaRole != ServerRoleId ||
            ArenaExperience != ExperienceId)
        {
            return EDivineBeastsProjectContextError::MainArenaExperienceMismatch;
        }
    }
    else if (
        ServerRoleId == FDivineBeastsProjectCatalog::GetMainArenaServerRole() &&
        ExperienceId != FDivineBeastsProjectCatalog::GetMainArenaExperience())
    {
        return EDivineBeastsProjectContextError::MainArenaExperienceMismatch;
    }

    if (!FDivineBeastsContractVersion::IsClientServerCompatible(ContractVersion))
    {
        return EDivineBeastsProjectContextError::InvalidContractVersion;
    }
    if (EnvironmentId.IsNone())
    {
        return EDivineBeastsProjectContextError::MissingEnvironmentId;
    }
    if (BuildVersion.IsEmpty())
    {
        return EDivineBeastsProjectContextError::MissingBuildVersion;
    }

    return EDivineBeastsProjectContextError::None;
}
