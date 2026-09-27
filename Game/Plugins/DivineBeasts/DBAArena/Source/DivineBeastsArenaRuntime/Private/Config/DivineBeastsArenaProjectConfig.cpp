#include "Config/DivineBeastsArenaProjectConfig.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Catalog/DivineBeastsHeroCatalog.h"

bool FDivineBeastsArenaProjectModeSpec::ValidateStructure(FString& OutError) const
{
    const FDivineBeastsArenaProjectModeSpec* Canonical =
        FDivineBeastsArenaModeCatalog::Find(ArenaModeId);
    if (!Canonical)
    {
        OutError = TEXT("ArenaModeId不是神兽联盟五种正式竞技模式。");
        return false;
    }
    if (TeamCount != 2 ||
        TeamSize != Canonical->TeamSize ||
        TotalPlayers != TeamCount * TeamSize ||
        ServerRoleId != FName(TEXT("GameServer.Role.MainArena")) ||
        ExperienceId != FName(TEXT("Experience.MainArena.Main")))
    {
        OutError = TEXT("ArenaMode结构人数/Role/Experience不符合项目固定约束。");
        return false;
    }
    if (HeroCatalogRevision != FDivineBeastsHeroCatalog::CatalogRevision)
    {
        OutError = TEXT("HeroCatalogRevision与DivineBeastsCharacters不一致。");
        return false;
    }
    OutError.Reset();
    return true;
}

bool FDivineBeastsArenaProjectModeSpec::ValidateProduction(FString& OutError) const
{
    if (!ValidateStructure(OutError))
    {
        return false;
    }
    if (ConfigState != EDivineBeastsArenaConfigState::ProductionReady)
    {
        OutError = TEXT("项目竞技模式尚未标记ProductionReady。");
        return false;
    }
    if (MapId.IsNone() ||
        SelectionPolicyId.IsNone() ||
        SpawnPolicyId.IsNone() ||
        RespawnPolicyId.IsNone() ||
        ScorePolicyId.IsNone() ||
        WinConditionPolicyId.IsNone() ||
        TimeLimitSeconds <= 0 ||
        ProjectRuleRevision <= 0 ||
        ContentRevision.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("Production Arena配置缺少Map/Policy/Time/Revision/ContentRevision。");
        return false;
    }
    if (PickBanState == EDivineBeastsArenaFeatureState::NotConfigured ||
        OvertimeState == EDivineBeastsArenaFeatureState::NotConfigured ||
        SuddenDeathState == EDivineBeastsArenaFeatureState::NotConfigured ||
        DuplicateHeroPolicy == EDivineBeastsArenaDuplicateHeroPolicy::Unspecified)
    {
        OutError = TEXT("PickBan/DuplicateHero/Overtime/SuddenDeath必须在Release前显式确认。");
        return false;
    }
    if (OvertimeState == EDivineBeastsArenaFeatureState::Supported &&
        OvertimePolicyId.IsNone())
    {
        OutError = TEXT("Overtime标记Supported时必须配置OvertimePolicyId。");
        return false;
    }
    OutError.Reset();
    return true;
}

bool FDivineBeastsArenaProjectModeSpec::TryBuildPlatformSpec(
    FGamePlatformArenaModeSpec& OutSpec,
    FString& OutError) const
{
    if (!ValidateProduction(OutError))
    {
        return false;
    }
    OutSpec = FGamePlatformArenaModeSpec{};
    OutSpec.ArenaModeId = ArenaModeId;
    OutSpec.TeamCount = TeamCount;
    OutSpec.TeamSize = TeamSize;
    OutSpec.TotalPlayers = TotalPlayers;
    OutSpec.MapId = MapId;
    OutSpec.MinPlayers = TotalPlayers;
    OutSpec.SelectionPolicyId = SelectionPolicyId;
    OutSpec.SpawnPolicyId = SpawnPolicyId;
    OutSpec.RespawnPolicyId = RespawnPolicyId;
    OutSpec.ScorePolicyId = ScorePolicyId;
    OutSpec.WinConditionPolicyId = WinConditionPolicyId;
    OutSpec.TimeLimitSeconds = TimeLimitSeconds;
    OutSpec.OvertimePolicyId =
        OvertimeState == EDivineBeastsArenaFeatureState::Supported
            ? OvertimePolicyId
            : NAME_None;
    OutSpec.Version = ProjectRuleRevision;
    return OutSpec.Validate(OutError);
}

FDivineBeastsArenaProjectModeSpec UDivineBeastsArenaModeDefinition::ToProjectSpec() const
{
    FDivineBeastsArenaProjectModeSpec Result;
    Result.ArenaModeId = ArenaModeId;
    Result.TeamCount = TeamCount;
    Result.TeamSize = TeamSize;
    Result.TotalPlayers = TotalPlayers;
    Result.ServerRoleId = ServerRoleId;
    Result.ExperienceId = ExperienceId;
    Result.MapId = MapId;
    Result.SelectionPolicyId = SelectionPolicyId;
    Result.SpawnPolicyId = SpawnPolicyId;
    Result.RespawnPolicyId = RespawnPolicyId;
    Result.ScorePolicyId = ScorePolicyId;
    Result.WinConditionPolicyId = WinConditionPolicyId;
    Result.OvertimePolicyId = OvertimePolicyId;
    Result.TimeLimitSeconds = TimeLimitSeconds;
    Result.ProjectRuleRevision = ProjectRuleRevision;
    Result.ContentRevision = ContentRevision;
    Result.HeroCatalogRevision = HeroCatalogRevision;
    Result.ConfigState = ConfigState;
    Result.PickBanState = PickBanState;
    Result.DuplicateHeroPolicy = DuplicateHeroPolicy;
    Result.OvertimeState = OvertimeState;
    Result.SuddenDeathState = SuddenDeathState;
    return Result;
}

bool UDivineBeastsArenaModeDefinition::ValidateProjectDefinition(
    bool bRequireProduction,
    FString& OutError) const
{
    const FDivineBeastsArenaProjectModeSpec Spec = ToProjectSpec();
    return bRequireProduction
        ? Spec.ValidateProduction(OutError)
        : Spec.ValidateStructure(OutError);
}
