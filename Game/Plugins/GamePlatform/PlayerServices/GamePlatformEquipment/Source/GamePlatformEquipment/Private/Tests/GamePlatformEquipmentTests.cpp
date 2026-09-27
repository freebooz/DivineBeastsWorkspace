#if WITH_DEV_AUTOMATION_TESTS

#include "Components/GamePlatformEquipmentComponent.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformEquipmentDefinitionTest,
    "GamePlatform.Equipment.DefinitionAndPublicState",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformEquipmentDefinitionTest::RunTest(const FString&)
{
    UGamePlatformEquipmentDefinition* Definition =
        NewObject<UGamePlatformEquipmentDefinition>();

    Definition->EquipmentDefinitionId =
        TEXT("Development.Equipment.FoundationSword");
    Definition->CompatibleItemDefinitionId =
        TEXT("Development.Inventory.UniqueTest");
    Definition->AllowedSlotIds = {TEXT("Weapon.Main")};
    Definition->VisualDefinitionId =
        TEXT("Development.EquipmentVisual.FoundationSword");

    TestTrue(
        TEXT("Definition结构有效"),
        Definition->IsStructurallyValid());

    TestTrue(
        TEXT("Slot兼容"),
        Definition->SupportsSlot(TEXT("Weapon.Main")));

    TestFalse(
        TEXT("错误Slot拒绝"),
        Definition->SupportsSlot(TEXT("Armor.Head")));

    return true;
}

#endif
