#include "Layers/DivineBeastsWorldHUDRoute.h"
#include "Identity/DivineBeastsProjectCatalog.h"

bool FDivineBeastsWorldHUDRoute::IsSupportedWorldRole(FName ServerRole)
{
    return ServerRole == FDivineBeastsProjectCatalog::GetOpenWorldServerRole() ||
           ServerRole == FDivineBeastsProjectCatalog::GetVillageServerRole();
}

int32 FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(
    FName ServerRole, FName ExperienceId)
{
    if (!IsSupportedWorldRole(ServerRole))
    {
        return INDEX_NONE;
    }
    // 按已保存Widget树的真实槽位顺序取值，保持与重构前完全相同的优先级。
    int32 Index = ServerRole == FDivineBeastsProjectCatalog::GetOpenWorldServerRole() ? 0 : 1;
    if (ExperienceId == FDivineBeastsProjectCatalog::GetVillageTutorialExperience())
    {
        Index = 2;
    }
    if (ExperienceId == FDivineBeastsProjectCatalog::GetVillageTrainingExperience())
    {
        Index = 3;
    }
    return Index;
}
