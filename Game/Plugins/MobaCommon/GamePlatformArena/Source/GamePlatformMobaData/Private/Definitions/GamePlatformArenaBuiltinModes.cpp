#include "Definitions/GamePlatformArenaBuiltinModes.h"

const FName FGamePlatformArenaBuiltinModes::Duel1v1(TEXT("Arena.Mode.Duel1v1"));
const FName FGamePlatformArenaBuiltinModes::Team2v2(TEXT("Arena.Mode.Team2v2"));
const FName FGamePlatformArenaBuiltinModes::Team3v3(TEXT("Arena.Mode.Team3v3"));
const FName FGamePlatformArenaBuiltinModes::Team4v4(TEXT("Arena.Mode.Team4v4"));
const FName FGamePlatformArenaBuiltinModes::Team5v5(TEXT("Arena.Mode.Team5v5"));
const FName FGamePlatformArenaBuiltinModes::MainArenaServerRole(TEXT("GameServer.Role.MainArena"));

namespace
{
    FGamePlatformArenaModeSpec MakeMode(const TCHAR* Id, int32 TeamSize)
    {
        FGamePlatformArenaModeSpec Mode;
        Mode.ArenaModeId = FName(Id);
        Mode.TeamCount = 2;
        Mode.TeamSize = TeamSize;
        Mode.TotalPlayers = TeamSize * 2;
        Mode.MinPlayers = TeamSize * 2;
        Mode.MapId = TEXT("Arena.Map.FoundationTest");
        Mode.SelectionPolicyId = TEXT("Arena.Selection.Standard");
        Mode.SpawnPolicyId = TEXT("Arena.Spawn.TwoTeam");
        Mode.RespawnPolicyId = TEXT("Arena.Respawn.Standard");
        Mode.ScorePolicyId = TEXT("Arena.Score.Standard");
        Mode.WinConditionPolicyId = TEXT("Arena.Win.Standard");
        Mode.OvertimePolicyId = TEXT("Arena.Overtime.None");
        Mode.TimeLimitSeconds = 900;
        Mode.Version = 1;
        return Mode;
    }
}

const TArray<FGamePlatformArenaModeSpec>& FGamePlatformArenaBuiltinModes::GetAll()
{
    static const TArray<FGamePlatformArenaModeSpec> Modes = {
        MakeMode(TEXT("Arena.Mode.Duel1v1"), 1),
        MakeMode(TEXT("Arena.Mode.Team2v2"), 2),
        MakeMode(TEXT("Arena.Mode.Team3v3"), 3),
        MakeMode(TEXT("Arena.Mode.Team4v4"), 4),
        MakeMode(TEXT("Arena.Mode.Team5v5"), 5)
    };
    return Modes;
}

const FGamePlatformArenaModeSpec* FGamePlatformArenaBuiltinModes::Find(FName ArenaModeId)
{
    return GetAll().FindByPredicate([ArenaModeId](const FGamePlatformArenaModeSpec& Mode)
    {
        return Mode.ArenaModeId == ArenaModeId;
    });
}

bool FGamePlatformArenaBuiltinModes::ValidateAll(FString& OutError)
{
    TSet<FName> Ids;
    for (const FGamePlatformArenaModeSpec& Mode : GetAll())
    {
        if (!Mode.Validate(OutError)) { return false; }
        if (Ids.Contains(Mode.ArenaModeId))
        {
            OutError = FString::Printf(TEXT("竞技模式ID重复：%s"), *Mode.ArenaModeId.ToString());
            return false;
        }
        Ids.Add(Mode.ArenaModeId);
    }
    if (Ids.Num() != 5)
    {
        OutError = TEXT("正式竞技模式必须恰好包含1v1、2v2、3v3、4v4、5v5五种定义。");
        return false;
    }
    OutError.Reset();
    return true;
}
