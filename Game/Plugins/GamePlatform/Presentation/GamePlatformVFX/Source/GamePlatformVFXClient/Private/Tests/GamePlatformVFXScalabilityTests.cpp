#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXHardBudgetTest,
    "GamePlatform.VFX.Scalability.HardBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXHardBudgetTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->MaxActiveInstances = 8;
    Settings->MaxStatusInstances = 6;
    Settings->MaxAmbientInstances = 3;
    Settings->HardMaxTrackedInstances = 10;

    FGamePlatformVFXRequest Ambient;
    Ambient.Importance = EGamePlatformVFXImportance::Ambient;
    TestTrue(
        TEXT("Ambient在自身软预算内允许"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Ambient, 2, *Settings));
    TestFalse(
        TEXT("Ambient达到低优先级软预算后拒绝，为战斗预留容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Ambient, 3, *Settings));

    FGamePlatformVFXRequest Status;
    Status.Importance = EGamePlatformVFXImportance::Status;
    TestTrue(
        TEXT("Status可以使用Ambient保留区之后的容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Status, 3, *Settings));
    TestFalse(
        TEXT("Status达到自身累计软预算后拒绝"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Status, 6, *Settings));

    FGamePlatformVFXRequest Combat;
    Combat.Importance = EGamePlatformVFXImportance::Combat;
    TestTrue(
        TEXT("Combat可以使用Status预算之后的保留容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Combat, 6, *Settings));
    TestFalse(
        TEXT("Combat达到普通总软预算后拒绝"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Combat, 8, *Settings));

    FGamePlatformVFXRequest Critical;
    Critical.Importance = EGamePlatformVFXImportance::Critical;
    TestTrue(
        TEXT("Critical允许突破普通软预算"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 8, *Settings));
    TestFalse(
        TEXT("Critical也不得突破绝对硬上限"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 10, *Settings));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCriticalPoolingTest,
    "GamePlatform.VFX.Scalability.CriticalPooling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCriticalPoolingTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->bEnablePooling = true;

    const UGamePlatformVFXInstantDefinition* Definition =
        NewObject<UGamePlatformVFXInstantDefinition>();

    FGamePlatformVFXRequest Critical;
    Critical.Importance = EGamePlatformVFXImportance::Critical;

    TestTrue(
        TEXT("Critical重要度不能强制关闭Niagara原生池"),
        FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(
            *Definition,
            Critical,
            *Settings));
    return true;
}

#endif
