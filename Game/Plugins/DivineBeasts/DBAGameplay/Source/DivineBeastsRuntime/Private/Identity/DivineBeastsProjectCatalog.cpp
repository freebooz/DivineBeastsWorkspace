#include "Identity/DivineBeastsProjectCatalog.h"

#include "Version/DivineBeastsGeneratedCatalogAdapter.h"

FName FDivineBeastsProjectCatalog::GetGameId()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetGameId();
}

FName FDivineBeastsProjectCatalog::GetProjectId()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetProjectId();
}

const TArray<FName>& FDivineBeastsProjectCatalog::GetServerRoleIds()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetServerRoleIds();
}

const TArray<FName>& FDivineBeastsProjectCatalog::GetExperienceIds()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetExperienceIds();
}

const TArray<FName>& FDivineBeastsProjectCatalog::GetArenaModeIds()
{
    return FDivineBeastsGeneratedCatalogAdapter::GetArenaModeIds();
}

bool FDivineBeastsProjectCatalog::IsServerRoleId(FName ServerRoleId)
{
    return GetServerRoleIds().Contains(ServerRoleId);
}

bool FDivineBeastsProjectCatalog::IsExperienceId(FName ExperienceId)
{
    return GetExperienceIds().Contains(ExperienceId);
}

bool FDivineBeastsProjectCatalog::IsArenaModeId(FName ArenaModeId)
{
    return GetArenaModeIds().Contains(ArenaModeId);
}

bool FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
    FName ExperienceId,
    FName& OutServerRoleId)
{
    return FDivineBeastsGeneratedCatalogAdapter::TryGetServerRoleForExperience(
        ExperienceId,
        OutServerRoleId);
}

bool FDivineBeastsProjectCatalog::TryGetArenaContextForMode(
    FName ArenaModeId,
    FName& OutServerRoleId,
    FName& OutExperienceId)
{
    return FDivineBeastsGeneratedCatalogAdapter::TryGetArenaContextForMode(
        ArenaModeId,
        OutServerRoleId,
        OutExperienceId);
}

namespace
{
    FName FindBySuffix(const TArray<FName>& Values, const TCHAR* Suffix)
    {
        for (const FName Value : Values)
        {
            if (Value.ToString().EndsWith(Suffix, ESearchCase::CaseSensitive))
            {
                return Value;
            }
        }
        return NAME_None;
    }
}

FName FDivineBeastsProjectCatalog::GetOpenWorldServerRole()
{
    static const FName Cached = FindBySuffix(GetServerRoleIds(), TEXT(".OpenWorld"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetVillageServerRole()
{
    static const FName Cached = FindBySuffix(GetServerRoleIds(), TEXT(".Village"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetMainArenaServerRole()
{
    static const FName Cached = FindBySuffix(GetServerRoleIds(), TEXT(".MainArena"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetOpenWorldHubExperience()
{
    // 缓存稳定目录身份，避免每次角色选择/赛后返回时重复遍历和FString构造。
    static const FName Cached = FindBySuffix(GetExperienceIds(), TEXT(".OpenWorld.Hub"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetVillageTutorialExperience()
{
    static const FName Cached = FindBySuffix(GetExperienceIds(), TEXT(".Village.Tutorial"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetVillageTrainingExperience()
{
    static const FName Cached = FindBySuffix(GetExperienceIds(), TEXT(".Village.Training"));
    return Cached;
}

FName FDivineBeastsProjectCatalog::GetMainArenaExperience()
{
    static const FName Cached = FindBySuffix(GetExperienceIds(), TEXT(".MainArena.Main"));
    return Cached;
}
