#pragma once

#include "CoreMinimal.h"

/**
 * FDivineBeastsProjectCatalog（神兽联盟项目核心目录）。
 * 所有值来自Shared生成Catalog；本类不维护第二份手写ID真源。
 */
class DIVINEBEASTSRUNTIME_API FDivineBeastsProjectCatalog
{
public:
    static FName GetGameId();
    static FName GetProjectId();

    static const TArray<FName>& GetServerRoleIds();
    static const TArray<FName>& GetExperienceIds();
    static const TArray<FName>& GetArenaModeIds();

    static bool IsServerRoleId(FName ServerRoleId);
    static bool IsExperienceId(FName ExperienceId);
    static bool IsArenaModeId(FName ArenaModeId);

    static bool TryGetServerRoleForExperience(
        FName ExperienceId,
        FName& OutServerRoleId);

    static bool TryGetArenaContextForMode(
        FName ArenaModeId,
        FName& OutServerRoleId,
        FName& OutExperienceId);

    static FName GetOpenWorldServerRole();
    static FName GetVillageServerRole();
    static FName GetMainArenaServerRole();
    /** 返回登录大厅/主城所在的OpenWorld默认体验；稳定身份集中于项目目录，避免业务模块散落字符串常量。 */
    static FName GetOpenWorldHubExperience();
    /** 返回首次新手引导体验；它属于Village服务器角色，不代表独立Tutorial服务器。 */
    static FName GetVillageTutorialExperience();
    /** 返回训练体验；它属于Village服务器角色，不代表独立Training服务器。 */
    static FName GetVillageTrainingExperience();
    static FName GetMainArenaExperience();
};
