#include "Definitions/GamePlatformArenaModeDefinition.h"

bool FGamePlatformArenaModeSpec::Validate(FString& OutError) const
{
    if (ArenaModeId.IsNone()) { OutError = TEXT("ArenaModeId不能为空。"); return false; }
    if (TeamCount != 2) { OutError = TEXT("第一版正式竞技模式TeamCount必须为2。"); return false; }
    if (TeamSize < 1 || TeamSize > 5) { OutError = TEXT("TeamSize必须在1到5之间。"); return false; }
    if (TotalPlayers != TeamCount * TeamSize) { OutError = TEXT("TeamCount * TeamSize必须等于TotalPlayers。"); return false; }
    if (MinPlayers < 1 || MinPlayers > TotalPlayers) { OutError = TEXT("MinPlayers超出合法范围。"); return false; }
    if (MapId.IsNone()) { OutError = TEXT("MapId不能为空。"); return false; }
    if (TimeLimitSeconds <= 0) { OutError = TEXT("TimeLimitSeconds必须大于0。"); return false; }
    if (Version <= 0) { OutError = TEXT("Version必须大于0。"); return false; }
    OutError.Reset();
    return true;
}

FPrimaryAssetId UGamePlatformArenaModeDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArenaMode")), ArenaModeId);
}

FGamePlatformArenaModeSpec UGamePlatformArenaModeDefinition::ToSpec() const
{
    FGamePlatformArenaModeSpec Spec;
    Spec.ArenaModeId = ArenaModeId;
    Spec.TeamCount = TeamCount;
    Spec.TeamSize = TeamSize;
    Spec.TotalPlayers = TotalPlayers;
    Spec.MapId = MapId;
    Spec.MinPlayers = MinPlayers;
    Spec.SelectionPolicyId = SelectionPolicyId;
    Spec.SpawnPolicyId = SpawnPolicyId;
    Spec.RespawnPolicyId = RespawnPolicyId;
    Spec.ScorePolicyId = ScorePolicyId;
    Spec.WinConditionPolicyId = WinConditionPolicyId;
    Spec.TimeLimitSeconds = TimeLimitSeconds;
    Spec.OvertimePolicyId = OvertimePolicyId;
    Spec.Version = Version;
    return Spec;
}

bool UGamePlatformArenaModeDefinition::ValidateDefinition(FString& OutError) const
{
    return ToSpec().Validate(OutError);
}
