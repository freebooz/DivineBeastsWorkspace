#pragma once

#include "CoreMinimal.h"

/**
 * FDivineBeastsGeneratedCatalogAdapter（Shared生成目录到UE运行时适配器）。
 * 仅Private使用，隔离标准C++生成头与公开UE类型。
 */
class FDivineBeastsGeneratedCatalogAdapter
{
public:
    static FName GetGameId();
    static FName GetProjectId();
    static FString GetContractVersion();
    static int32 GetCatalogVersion();
    static FString GetGeneratedRevision();

    static const TArray<FName>& GetServerRoleIds();
    static const TArray<FName>& GetExperienceIds();
    static const TArray<FName>& GetArenaModeIds();
    /** 返回Shared生成的十二生肖稳定HeroDefinitionId集合。 */
    static const TArray<FName>& GetHeroDefinitionIds();

    static bool TryGetServerRoleForExperience(
        FName ExperienceId,
        FName& OutServerRoleId);

    static bool TryGetArenaContextForMode(
        FName ArenaModeId,
        FName& OutServerRoleId,
        FName& OutExperienceId);

    static FString GetClientServerMinimumVersion();
    static FString GetClientServerMaximumExclusiveVersion();
    static FString GetServerBackendMinimumVersion();
    static FString GetServerBackendMaximumExclusiveVersion();
};
