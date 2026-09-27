#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/GamePlatformEquipmentVisualDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformEquipmentVisualDefinitionTest,
    "GamePlatform.Equipment.Client.VisualDefinition",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformEquipmentVisualDefinitionTest::RunTest(const FString&)
{
    UGamePlatformEquipmentVisualDefinition* Definition =
        NewObject<UGamePlatformEquipmentVisualDefinition>();

    Definition->EquipmentVisualId =
        TEXT("Development.EquipmentVisual.FoundationSword");
    Definition->SocketName = TEXT("Weapon_R");

    TestEqual(
        TEXT("第一版视觉类型为StaticMesh"),
        Definition->VisualType,
        EGamePlatformEquipmentVisualType::StaticMesh);

    TestEqual(
        TEXT("Socket由Definition驱动"),
        Definition->SocketName,
        FName(TEXT("Weapon_R")));

    return true;
}

#endif
