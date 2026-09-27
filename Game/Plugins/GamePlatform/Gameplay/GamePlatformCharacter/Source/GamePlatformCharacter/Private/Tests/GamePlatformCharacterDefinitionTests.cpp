#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformHeroDefinition.h"
#include "Initialization/GamePlatformCharacterInitializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCharacterDefinitionValueTest,
    "GamePlatform.Character.DefinitionValueValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCharacterDefinitionValueTest::RunTest(const FString& Parameters)
{
    FString Error;

    FGamePlatformCharacterSpawnEnvelope Spawn;
    TestTrue(TEXT("默认SpawnEnvelope有效"), Spawn.IsValid(Error));
    Spawn.CapsuleHalfHeight = 20.0f;
    Spawn.CapsuleRadius = 42.0f;
    TestFalse(TEXT("半高小于半径被拒绝"), Spawn.IsValid(Error));

    FGamePlatformCharacterMovementDefinition Movement;
    TestTrue(TEXT("默认Movement有效"), Movement.IsValid(Error));
    Movement.MaxWalkSpeed = 0.0f;
    TestFalse(TEXT("非正最大速度被拒绝"), Movement.IsValid(Error));

    FGamePlatformCharacterInitializationContext Context;
    Context.HeroDefinitionId = TEXT("Hero.Test");
    Context.SpawnGeneration = 1;
    Context.AvatarGeneration = 1;
    Context.CharacterId = TEXT("Character-1");
    TestTrue(TEXT("持久角色初始化上下文有效"), Context.IsValid(Error));

    Context.CharacterId.Reset();
    TestFalse(TEXT("持久角色缺CharacterId被拒绝"), Context.IsValid(Error));
    Context.bPersistentCharacterIdRequired = false;
    TestTrue(TEXT("AI等非持久Avatar可无CharacterId"), Context.IsValid(Error));
    return true;
}

#endif
