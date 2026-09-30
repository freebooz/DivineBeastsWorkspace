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
    TestTrue(TEXT("Composite 默认总生命周期必须大于0，避免无完成事件的父实例永久残留"), Definition->MaxTotalLifetimeSeconds > 0.0f);
    return true;
}

#endif
