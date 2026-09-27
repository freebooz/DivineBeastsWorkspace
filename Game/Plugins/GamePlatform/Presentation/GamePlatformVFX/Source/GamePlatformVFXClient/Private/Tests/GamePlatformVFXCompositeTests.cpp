#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCompositeDefinitionDefaultsTest,
    "GamePlatform.VFX.Composite.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCompositeDefinitionDefaultsTest::RunTest(const FString& Parameters)
{
    const UGamePlatformVFXCompositeDefinition* Definition = NewObject<UGamePlatformVFXCompositeDefinition>();
    TestEqual(
        TEXT("Composite 行为类型正确"),
        Definition->GetBehavior(),
        EGamePlatformVFXBehavior::Composite);
    TestFalse(TEXT("Composite 默认不使用 Niagara 池"), Definition->AllowsPooling());
    return true;
}

#endif
