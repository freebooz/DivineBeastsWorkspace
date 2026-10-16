#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Arena/GamePlatformArenaTypes.h"
#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Catalog/DivineBeastsHeroCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaFiveModesTest,
    "DivineBeasts.Arena.ProjectCatalog.FiveModes",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaFiveModesTest::RunTest(const FString&)
{
    const TArray<FDivineBeastsArenaProjectModeSpec>& Modes =
        FDivineBeastsArenaModeCatalog::GetAll();
    TestEqual(TEXT("Exactly five project arena modes"), Modes.Num(), 5);

    const TArray<FName> ExpectedIds =
    {
        TEXT("Arena.Mode.Duel1v1"),
        TEXT("Arena.Mode.Team2v2"),
        TEXT("Arena.Mode.Team3v3"),
        TEXT("Arena.Mode.Team4v4"),
        TEXT("Arena.Mode.Team5v5")
    };

    for (int32 Index = 0; Index < ExpectedIds.Num(); ++Index)
    {
        const FDivineBeastsArenaProjectModeSpec* Spec =
            FDivineBeastsArenaModeCatalog::Find(ExpectedIds[Index]);
        TestNotNull(TEXT("Mode exists"), Spec);
        if (!Spec) { continue; }

        TestEqual(TEXT("TeamCount"), Spec->TeamCount, 2);
        TestEqual(TEXT("TeamSize"), Spec->TeamSize, Index + 1);
        TestEqual(TEXT("TotalPlayers"), Spec->TotalPlayers, (Index + 1) * 2);
        TestEqual(
            TEXT("MainArena role"),
            Spec->ServerRoleId,
            FName(TEXT("GameServer.Role.MainArena")));
        TestEqual(
            TEXT("MainArena experience"),
            Spec->ExperienceId,
            FName(TEXT("Experience.MainArena.Main")));
        TestEqual(
            TEXT("Hero catalog revision"),
            Spec->HeroCatalogRevision,
            FDivineBeastsHeroCatalog::CatalogRevision);

        FString Error;
        TestTrue(TEXT("Structural definition valid"), Spec->ValidateStructure(Error));
        Error.Reset();
        TestFalse(TEXT("Production remains blocked until approved"), Spec->ValidateProduction(Error));
        TestTrue(TEXT("Production failure reason present"), !Error.IsEmpty());
        // 已保存的开发地图不是生产批准：显式开发参数可用，默认生产门禁仍必须拒绝。
        TestEqual(TEXT("仅指向已交付MainArena开发地图"),Spec->MapId,FName(TEXT("divinebeasts.world.main_arena@1")));
        TestTrue(TEXT("显式开发运行可解析该模式"),Spec->ValidateRuntime(true,Error));
        TestFalse(TEXT("未启用开发时仍拒绝"),Spec->ValidateRuntime(false,Error));
    }

    FString CatalogError;
    TestTrue(
        TEXT("Structural catalog valid"),
        FDivineBeastsArenaModeCatalog::ValidateStructuralCatalog(CatalogError));
    CatalogError.Reset();
    TestFalse(
        TEXT("Production catalog blocked"),
        FDivineBeastsArenaModeCatalog::ValidateProductionCatalog(CatalogError));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaAssignmentProductionGateTest,
    "DivineBeasts.Arena.ProjectCatalog.AssignmentProductionGate",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaAssignmentProductionGateTest::RunTest(const FString&)
{
    FGamePlatformArenaAssignment Assignment;
    Assignment.MatchId = TEXT("test-match");
    Assignment.ArenaModeId = TEXT("Arena.Mode.Duel1v1");
    Assignment.MapId = TEXT("Arena.Map.FoundationTest");
    Assignment.ServerRole = TEXT("GameServer.Role.MainArena");
    Assignment.ExperienceId = TEXT("Experience.MainArena.Main");
    Assignment.GameServerId = TEXT("server-test");
    Assignment.TeamSize = 1;
    Assignment.TotalPlayers = 2;

    FGamePlatformArenaRosterSlot A;
    A.PlayerId = TEXT("p1");
    A.CharacterId = TEXT("c1");
    A.TeamId = TEXT("Arena.Team.1");
    A.SlotIndex = 0;
    Assignment.Roster.Add(A);

    FGamePlatformArenaRosterSlot B;
    B.PlayerId = TEXT("p2");
    B.CharacterId = TEXT("c2");
    B.TeamId = TEXT("Arena.Team.2");
    B.SlotIndex = 1;
    Assignment.Roster.Add(B);

    FGamePlatformArenaModeSpec OutSpec;
    FString Error;
    TestFalse(
        TEXT("Foundation test assignment must not pass project production gate"),
        FDivineBeastsArenaModeCatalog::ValidateAssignmentAgainstProduction(
            Assignment,
            OutSpec,
            Error));
    TestTrue(TEXT("Failure reason present"), !Error.IsEmpty());
    return true;
}

#endif
