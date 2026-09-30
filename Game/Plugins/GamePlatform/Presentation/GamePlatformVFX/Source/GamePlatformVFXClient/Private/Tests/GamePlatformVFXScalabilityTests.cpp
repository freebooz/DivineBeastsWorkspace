#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXHardBudgetTest,
    "GamePlatform.VFX.Scalability.HardBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXHardBudgetTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->MaxActiveInstances = 4;
    Settings->HardMaxTrackedInstances = 8;

    FGamePlatformVFXRequest Normal;
    Normal.Importance = EGamePlatformVFXImportance::Combat;
    TestFalse(
        TEXT("普通请求达到软预算后应拒绝"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Normal, 4, *Settings));

    FGamePlatformVFXRequest Critical;
    Critical.Importance = EGamePlatformVFXImportance::Critical;
    TestTrue(
        TEXT("Critical允许突破软预算"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 4, *Settings));
    TestFalse(
        TEXT("Critical也不得突破绝对硬上限"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 8, *Settings));
    return true;
}

#endif
