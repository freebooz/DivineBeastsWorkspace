#include "Version/DivineBeastsGeneratedCatalogAdapter.h"

#include "DivineBeastsCatalog.generated.hpp"

namespace
{
    FName ToName(const char* Value)
    {
        return Value ? FName(UTF8_TO_TCHAR(Value)) : NAME_None;
    }

    FString ToStringValue(const char* Value)
    {
        return Value ? FString(UTF8_TO_TCHAR(Value)) : FString();
    }

    template <typename TArrayType>
    TArray<FName> MakeNames(const TArrayType& Values)
    {
        TArray<FName> Result;
        Result.Reserve(static_cast<int32>(Values.size()));
        for (const char* Value : Values)
        {
            Result.Add(ToName(Value));
        }
        return Result;
    }
}

FName FDivineBeastsGeneratedCatalogAdapter::GetGameId()
{
    return ToName(DivineBeasts::Contracts::GameId);
}

FName FDivineBeastsGeneratedCatalogAdapter::GetProjectId()
{
    return ToName(DivineBeasts::Contracts::ProjectId);
}

FString FDivineBeastsGeneratedCatalogAdapter::GetContractVersion()
{
    return ToStringValue(DivineBeasts::Contracts::ContractVersion);
}

int32 FDivineBeastsGeneratedCatalogAdapter::GetCatalogVersion()
{
    return DivineBeasts::Contracts::CatalogVersion;
}

FString FDivineBeastsGeneratedCatalogAdapter::GetGeneratedRevision()
{
    return ToStringValue(DivineBeasts::Contracts::GeneratedRevision);
}

const TArray<FName>& FDivineBeastsGeneratedCatalogAdapter::GetServerRoleIds()
{
    static const TArray<FName> Values = MakeNames(DivineBeasts::Contracts::ServerRoles);
    return Values;
}

const TArray<FName>& FDivineBeastsGeneratedCatalogAdapter::GetExperienceIds()
{
    static const TArray<FName> Values = MakeNames(DivineBeasts::Contracts::ExperienceIds);
    return Values;
}

const TArray<FName>& FDivineBeastsGeneratedCatalogAdapter::GetArenaModeIds()
{
    static const TArray<FName> Values = MakeNames(DivineBeasts::Contracts::ArenaModeIds);
    return Values;
}

const TArray<FName>& FDivineBeastsGeneratedCatalogAdapter::GetHeroDefinitionIds()
{
    static const TArray<FName> Values = MakeNames(DivineBeasts::Contracts::HeroDefinitionIds);
    return Values;
}

bool FDivineBeastsGeneratedCatalogAdapter::TryGetServerRoleForExperience(
    FName ExperienceId,
    FName& OutServerRoleId)
{
    for (const auto& Mapping : DivineBeasts::Contracts::ExperienceMappings)
    {
        if (ToName(Mapping.ExperienceId) == ExperienceId)
        {
            OutServerRoleId = ToName(Mapping.ServerRoleId);
            return true;
        }
    }
    OutServerRoleId = NAME_None;
    return false;
}

bool FDivineBeastsGeneratedCatalogAdapter::TryGetArenaContextForMode(
    FName ArenaModeId,
    FName& OutServerRoleId,
    FName& OutExperienceId)
{
    for (const auto& Mapping : DivineBeasts::Contracts::ArenaModeMappings)
    {
        if (ToName(Mapping.ArenaModeId) == ArenaModeId)
        {
            OutServerRoleId = ToName(Mapping.ServerRoleId);
            OutExperienceId = ToName(Mapping.ExperienceId);
            return true;
        }
    }
    OutServerRoleId = NAME_None;
    OutExperienceId = NAME_None;
    return false;
}

FString FDivineBeastsGeneratedCatalogAdapter::GetClientServerMinimumVersion()
{
    return ToStringValue(DivineBeasts::Contracts::ClientServerMinimumContractVersion);
}

FString FDivineBeastsGeneratedCatalogAdapter::GetClientServerMaximumExclusiveVersion()
{
    return ToStringValue(DivineBeasts::Contracts::ClientServerMaximumExclusiveContractVersion);
}

FString FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMinimumVersion()
{
    return ToStringValue(DivineBeasts::Contracts::ServerBackendMinimumContractVersion);
}

FString FDivineBeastsGeneratedCatalogAdapter::GetServerBackendMaximumExclusiveVersion()
{
    return ToStringValue(DivineBeasts::Contracts::ServerBackendMaximumExclusiveContractVersion);
}
