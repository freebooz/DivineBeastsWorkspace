#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "State/GamePlatformArenaMatchStateMachine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaModesTest,
    "GamePlatform.Arena.Modes.FiveDefinitions",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaModesTest::RunTest(const FString& Parameters)
{
    FString Error;
    TestTrue(TEXT("五种模式均合法"), FGamePlatformArenaBuiltinModes::ValidateAll(Error));
    const TArray<FGamePlatformArenaModeSpec>& Modes = FGamePlatformArenaBuiltinModes::GetAll();
    TestEqual(TEXT("模式数量"), Modes.Num(), 5);
    TestEqual(TEXT("1v1人数"), FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Duel1v1)->TotalPlayers, 2);
    TestEqual(TEXT("2v2人数"), FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team2v2)->TotalPlayers, 4);
    TestEqual(TEXT("3v3人数"), FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team3v3)->TotalPlayers, 6);
    TestEqual(TEXT("4v4人数"), FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team4v4)->TotalPlayers, 8);
    TestEqual(TEXT("5v5人数"), FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team5v5)->TotalPlayers, 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaInvalidModeTest,
    "GamePlatform.Arena.Modes.InvalidDefinitions",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaInvalidModeTest::RunTest(const FString& Parameters)
{
    FString Error;
    FGamePlatformArenaModeSpec BadTeamSize = *FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team5v5);
    BadTeamSize.TeamSize = 6;
    TestFalse(TEXT("TeamSize超过5必须失败"), BadTeamSize.Validate(Error));

    FGamePlatformArenaModeSpec BadTotal = *FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Team3v3);
    BadTotal.TotalPlayers = 5;
    TestFalse(TEXT("TotalPlayers与TeamCount*TeamSize不一致必须失败"), BadTotal.Validate(Error));
    TestEqual(TEXT("统一MainArena角色"), FGamePlatformArenaBuiltinModes::MainArenaServerRole, FName(TEXT("GameServer.Role.MainArena")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaPhaseTest,
    "GamePlatform.Arena.StateMachine.ValidTransitions",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaPhaseTest::RunTest(const FString& Parameters)
{
    FGamePlatformArenaMatchStateMachine Machine;
    FString Error;
    TestTrue(TEXT("等待Assignment"), Machine.TryTransition(EGamePlatformArenaMatchPhase::WaitingAssignment, Error));
    TestTrue(TEXT("Preparing"), Machine.TryTransition(EGamePlatformArenaMatchPhase::Preparing, Error));
    TestTrue(TEXT("WaitingPlayers"), Machine.TryTransition(EGamePlatformArenaMatchPhase::WaitingPlayers, Error));
    TestFalse(TEXT("禁止跳过阶段直接InProgress"), Machine.TryTransition(EGamePlatformArenaMatchPhase::InProgress, Error));
    return true;
}

#endif
