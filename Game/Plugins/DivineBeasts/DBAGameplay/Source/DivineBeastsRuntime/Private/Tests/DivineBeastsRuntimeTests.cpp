#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Context/DivineBeastsProjectContext.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Version/DivineBeastsContractVersion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsProjectCatalogTest,
    "DivineBeasts.Runtime.ProjectCatalog",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsProjectCatalogTest::RunTest(const FString&)
{
    TestEqual(TEXT("GameId"), FDivineBeastsProjectCatalog::GetGameId(), FName(TEXT("divinebeasts")));
    TestEqual(TEXT("ProjectId"), FDivineBeastsProjectCatalog::GetProjectId(), FName(TEXT("DivineBeastsArena")));
    TestEqual(TEXT("ServerRole count"), FDivineBeastsProjectCatalog::GetServerRoleIds().Num(), 3);
    TestEqual(TEXT("Experience count"), FDivineBeastsProjectCatalog::GetExperienceIds().Num(), 7);
    TestEqual(TEXT("ArenaMode count"), FDivineBeastsProjectCatalog::GetArenaModeIds().Num(), 5);

    TestFalse(TEXT("Lobby invalid"), FDivineBeastsProjectCatalog::IsServerRoleId(TEXT("GameServer.Role.Lobby")));
    TestTrue(TEXT("OpenWorld valid"), FDivineBeastsProjectCatalog::IsServerRoleId(TEXT("GameServer.Role.OpenWorld")));
    TestTrue(TEXT("Village valid"), FDivineBeastsProjectCatalog::IsServerRoleId(TEXT("GameServer.Role.Village")));
    TestTrue(TEXT("MainArena valid"), FDivineBeastsProjectCatalog::IsServerRoleId(TEXT("GameServer.Role.MainArena")));
    TestFalse(TEXT("Unknown role invalid"), FDivineBeastsProjectCatalog::IsServerRoleId(TEXT("GameServer.Role.Unknown")));

    FName Role = NAME_None;
    TestTrue(TEXT("Hub maps"), FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(TEXT("Experience.OpenWorld.Hub"), Role));
    TestEqual(TEXT("Hub -> OpenWorld"), Role, FName(TEXT("GameServer.Role.OpenWorld")));
    TestTrue(TEXT("Legacy Lobby experience maps"), FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(TEXT("Experience.Lobby.Main"), Role));
    TestEqual(TEXT("Legacy Lobby experience -> OpenWorld"), Role, FName(TEXT("GameServer.Role.OpenWorld")));
    TestTrue(TEXT("Tutorial maps"), FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(TEXT("Experience.Village.Tutorial"), Role));
    TestEqual(TEXT("Tutorial -> Village"), Role, FName(TEXT("GameServer.Role.Village")));
    TestEqual(TEXT("Hub getter"), FDivineBeastsProjectCatalog::GetOpenWorldHubExperience(), FName(TEXT("Experience.OpenWorld.Hub")));
    TestEqual(TEXT("Tutorial getter"), FDivineBeastsProjectCatalog::GetVillageTutorialExperience(), FName(TEXT("Experience.Village.Tutorial")));
    TestEqual(TEXT("Training getter"), FDivineBeastsProjectCatalog::GetVillageTrainingExperience(), FName(TEXT("Experience.Village.Training")));
    TestTrue(TEXT("Training maps"), FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(TEXT("Experience.Village.Training"), Role));
    TestEqual(TEXT("Training -> Village"), Role, FName(TEXT("GameServer.Role.Village")));

    for (const FName Mode : FDivineBeastsProjectCatalog::GetArenaModeIds())
    {
        FName ArenaRole = NAME_None;
        FName Experience = NAME_None;
        TestTrue(TEXT("Arena mode maps"), FDivineBeastsProjectCatalog::TryGetArenaContextForMode(Mode, ArenaRole, Experience));
        TestEqual(TEXT("Arena role"), ArenaRole, FName(TEXT("GameServer.Role.MainArena")));
        TestEqual(TEXT("Arena experience"), Experience, FName(TEXT("Experience.MainArena.Main")));
    }
    TestFalse(TEXT("Unknown ArenaMode invalid"), FDivineBeastsProjectCatalog::IsArenaModeId(TEXT("Arena.Mode.Unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsProjectContextTest,
    "DivineBeasts.Runtime.ProjectContext",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsProjectContextTest::RunTest(const FString&)
{
    FDivineBeastsProjectContext Context;
    Context.InitializeCanonicalIdentity();
    Context.ServerRoleId = TEXT("GameServer.Role.OpenWorld");
    Context.ExperienceId = TEXT("Experience.OpenWorld.Hub");
    Context.EnvironmentId = TEXT("Development");
    Context.BuildVersion = TEXT("TestBuild");
    TestTrue(TEXT("OpenWorld Hub context valid"), Context.IsValid());

    Context.ServerRoleId = TEXT("GameServer.Role.Lobby");
    TestFalse(TEXT("Removed Lobby role invalid"), Context.IsValid());

    Context.ServerRoleId = TEXT("GameServer.Role.OpenWorld");
    Context.ExperienceId = TEXT("Experience.OpenWorld.Hub");
    Context.EnvironmentId = TEXT("Development");
    Context.BuildVersion = TEXT("TestBuild");
    TestTrue(TEXT("OpenWorld context valid"), Context.IsValid());

    Context.ArenaModeId = TEXT("Arena.Mode.Duel1v1");
    TestEqual(
        TEXT("OpenWorld cannot carry ArenaMode"),
        Context.Validate(),
        EDivineBeastsProjectContextError::ArenaModeRequiresMainArena);

    Context.ServerRoleId = TEXT("GameServer.Role.MainArena");
    Context.ExperienceId = TEXT("Experience.MainArena.Main");
    TestTrue(TEXT("MainArena context valid"), Context.IsValid());

    Context.ServerRoleId = TEXT("GameServer.Role.Village");
    Context.ExperienceId = TEXT("Experience.Village.Tutorial");
    Context.ArenaModeId = NAME_None;
    TestTrue(TEXT("Village tutorial valid"), Context.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsContractVersionTest,
    "DivineBeasts.Runtime.ContractVersion",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsContractVersionTest::RunTest(const FString&)
{
	TestEqual(TEXT("Current contract version"), FDivineBeastsContractVersion::GetCurrentVersion(), FString(TEXT("2.0.0")));
	TestEqual(TEXT("Catalog version"), FDivineBeastsContractVersion::GetCatalogVersion(), 1);
	TestEqual(TEXT("Generated revision length"), FDivineBeastsContractVersion::GetGeneratedRevision().Len(), 64);
	TestTrue(TEXT("minimum supported version"), FDivineBeastsContractVersion::IsClientServerCompatible(TEXT("2.0.0")));
	TestFalse(TEXT("old independent-Lobby contract is incompatible"), FDivineBeastsContractVersion::IsClientServerCompatible(TEXT("1.4.0")));
	TestTrue(TEXT("latest 2.x patch is compatible"), FDivineBeastsContractVersion::IsClientServerCompatible(TEXT("2.9.9")));
	TestFalse(TEXT("next major is outside the half-open range"), FDivineBeastsContractVersion::IsClientServerCompatible(TEXT("3.0.0")));
	TestTrue(TEXT("server-backend supported version"), FDivineBeastsContractVersion::IsServerBackendCompatible(TEXT("2.0.0")));
	TestFalse(TEXT("bad semver incompatible"), FDivineBeastsContractVersion::IsClientServerCompatible(TEXT("bad")));
	return true;
}

#endif
