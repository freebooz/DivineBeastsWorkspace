#include "Catalog/DivineBeastsArenaModeCatalog.h"

#include "Arena/GamePlatformArenaTypes.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    FDivineBeastsArenaProjectModeSpec MakeStructuralMode(
        const TCHAR* ModeId,
        int32 TeamSize)
    {
        FDivineBeastsArenaProjectModeSpec Spec;
        Spec.ArenaModeId = FName(ModeId);
        Spec.TeamCount = 2;
        Spec.TeamSize = TeamSize;
        Spec.TotalPlayers = TeamSize * 2;
        Spec.ServerRoleId = TEXT("GameServer.Role.MainArena");
        Spec.ExperienceId = TEXT("Experience.MainArena.Main");
        Spec.HeroCatalogRevision = FDivineBeastsHeroCatalog::CatalogRevision;

        // 用户批准单局歼灭规则；当前白盒地图/候选技能明确为开发内容，不能自动标记正式发行。
        Spec.MapId=TEXT("divinebeasts.world.main_arena@1");
        Spec.SelectionPolicyId=TEXT("Arena.Selection.LockedRoster");
        Spec.SpawnPolicyId=TEXT("Arena.Spawn.RosterSlot");
        Spec.RespawnPolicyId=TEXT("Arena.Respawn.Disabled");
        Spec.ScorePolicyId=TEXT("Arena.Score.Eliminations");
        Spec.WinConditionPolicyId=TEXT("Arena.WinCondition.SingleRoundElimination");
        Spec.TimeLimitSeconds=1200;Spec.ProjectRuleRevision=1;Spec.ContentRevision=TEXT("MainArena.Development.20261015.1");
        Spec.ConfigState=EDivineBeastsArenaConfigState::DevelopmentOnly;
        Spec.PickBanState=EDivineBeastsArenaFeatureState::Unsupported;
        Spec.DuplicateHeroPolicy=EDivineBeastsArenaDuplicateHeroPolicy::Allowed;
        Spec.OvertimeState=EDivineBeastsArenaFeatureState::Unsupported;
        Spec.SuddenDeathState=EDivineBeastsArenaFeatureState::Unsupported;
        return Spec;
    }
}

const TArray<FDivineBeastsArenaProjectModeSpec>&
FDivineBeastsArenaModeCatalog::GetAll()
{
    static const TArray<FDivineBeastsArenaProjectModeSpec> Modes =
    {
        MakeStructuralMode(TEXT("Arena.Mode.Duel1v1"), 1),
        MakeStructuralMode(TEXT("Arena.Mode.Team2v2"), 2),
        MakeStructuralMode(TEXT("Arena.Mode.Team3v3"), 3),
        MakeStructuralMode(TEXT("Arena.Mode.Team4v4"), 4),
        MakeStructuralMode(TEXT("Arena.Mode.Team5v5"), 5)
    };
    return Modes;
}

const FDivineBeastsArenaProjectModeSpec*
FDivineBeastsArenaModeCatalog::Find(FName ArenaModeId)
{
    return GetAll().FindByPredicate(
        [ArenaModeId](const FDivineBeastsArenaProjectModeSpec& Spec)
        {
            return Spec.ArenaModeId == ArenaModeId;
        });
}

bool FDivineBeastsArenaModeCatalog::ValidateStructuralCatalog(FString& OutError)
{
    if (GetAll().Num() != 5)
    {
        OutError = TEXT("项目竞技Catalog必须恰好包含五种模式。");
        return false;
    }

    TSet<FName> Ids;
    for (const FDivineBeastsArenaProjectModeSpec& Spec : GetAll())
    {
        if (!Spec.ValidateStructure(OutError) || Ids.Contains(Spec.ArenaModeId))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("项目竞技Catalog存在重复ArenaModeId。");
            }
            return false;
        }
        Ids.Add(Spec.ArenaModeId);
    }
    OutError.Reset();
    return true;
}

bool FDivineBeastsArenaModeCatalog::ValidateProductionCatalog(FString& OutError)
{
    if (!ValidateStructuralCatalog(OutError))
    {
        return false;
    }
    for (const FDivineBeastsArenaProjectModeSpec& Spec : GetAll())
    {
        if (!Spec.ValidateProduction(OutError))
        {
            OutError = Spec.ArenaModeId.ToString() + TEXT(": ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool FDivineBeastsArenaModeCatalog::IsDevelopmentEnabledForCurrentProcess()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return false;
#else
    bool bConfigured=false;if(GConfig)GConfig->GetBool(TEXT("DivineBeasts.Arena"),TEXT("bAllowDevelopmentArena"),bConfigured,GGameIni);
    return bConfigured&&FParse::Param(FCommandLine::Get(),TEXT("DBADevelopmentArena"));
#endif
}

bool FDivineBeastsArenaModeCatalog::ValidateAssignmentAgainstProduction(const FGamePlatformArenaAssignment& Assignment,FGamePlatformArenaModeSpec& OutModeSpec,FString& OutError)
{return ValidateAssignmentForRuntime(Assignment,OutModeSpec,OutError,false);}

bool FDivineBeastsArenaModeCatalog::ValidateAssignmentForRuntime(
    const FGamePlatformArenaAssignment& Assignment,
    FGamePlatformArenaModeSpec& OutModeSpec,
    FString& OutError,bool bAllowDevelopment)
{
    const FDivineBeastsArenaProjectModeSpec* ProjectSpec =
        Find(Assignment.ArenaModeId);
    if (!ProjectSpec || !ProjectSpec->TryBuildPlatformSpec(OutModeSpec, OutError,bAllowDevelopment))
    {
        return false;
    }

    if (Assignment.ServerRole != ProjectSpec->ServerRoleId ||
        Assignment.ExperienceId != ProjectSpec->ExperienceId ||
        Assignment.MapId != ProjectSpec->MapId ||
        Assignment.TeamSize != ProjectSpec->TeamSize ||
        Assignment.TotalPlayers != ProjectSpec->TotalPlayers ||
        Assignment.ProjectRuleRevision != ProjectSpec->ProjectRuleRevision ||
        Assignment.ContentRevision != ProjectSpec->ContentRevision ||
        Assignment.HeroCatalogRevision != ProjectSpec->HeroCatalogRevision)
    {
        OutError = TEXT("Assignment与项目Production Mode Revision/Map/Content/HeroCatalog不一致。");
        return false;
    }

    TSet<FString> Players;
    TSet<FString> Characters;
    TMap<FName, int32> TeamCounts;
    for (const FGamePlatformArenaRosterSlot& Slot : Assignment.Roster)
    {
        if (!Slot.IsValid() ||
            Players.Contains(Slot.PlayerId) ||
            Characters.Contains(Slot.CharacterId))
        {
            OutError = TEXT("Assignment Roster存在无效或重复PlayerId/CharacterId。");
            return false;
        }
        Players.Add(Slot.PlayerId);
        Characters.Add(Slot.CharacterId);
        TeamCounts.FindOrAdd(Slot.TeamId)++;
    }
    if (Assignment.Roster.Num() != ProjectSpec->TotalPlayers ||
        TeamCounts.Num() != ProjectSpec->TeamCount)
    {
        OutError = TEXT("Assignment Roster人数或队伍数不正确。");
        return false;
    }
    for (const TPair<FName, int32>& Pair : TeamCounts)
    {
        if (Pair.Value != ProjectSpec->TeamSize)
        {
            OutError = TEXT("Assignment Roster每队人数不正确。");
            return false;
        }
    }

    OutError.Reset();
    return true;
}
