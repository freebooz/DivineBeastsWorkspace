#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformUIAccessibilityPreferencesTest,
    "GamePlatform.UI.Accessibility.PreferencesAndReducedMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIAccessibilityPreferencesTest::RunTest(const FString& Parameters)
{
    // LocalPlayer的ClassWithin是Engine；Transient包外层会触发Ensure，污染真实回归结果。
    if (!TestNotNull(TEXT("测试具有真实Engine外层"), GEngine)) return false;
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    UGamePlatformUIManagerSubsystem* Manager =
        NewObject<UGamePlatformUIManagerSubsystem>(LocalPlayer);

    FGamePlatformUIAccessibilityPreferences Preferences;
    Preferences.TextScale = 0.1f;
    Preferences.TouchTargetScale = 0.5f;
    Preferences.bReducedMotion = true;
    Preferences.bPreferHighContrast = true;

    Manager->SetAccessibilityPreferences(Preferences);
    const FGamePlatformUIAccessibilityPreferences Effective =
        Manager->GetAccessibilityPreferences();

    TestEqual(TEXT("文本缩放具有可读下限"), Effective.TextScale, 0.5f);
    TestEqual(TEXT("触控目标不能缩小默认尺寸"), Effective.TouchTargetScale, 1.0f);
    TestTrue(TEXT("高对比度偏好保持"), Effective.bPreferHighContrast);
    TestTrue(TEXT("默认要求非颜色状态线索"), Effective.bRequireNonColorStatusCues);
    TestEqual(
        TEXT("Reduced Motion将Default过渡降为Instant"),
        Manager->ResolveTransition(EGamePlatformUITransition::Default),
        EGamePlatformUITransition::Instant);
    TestEqual(
        TEXT("显式None过渡保持不变"),
        Manager->ResolveTransition(EGamePlatformUITransition::None),
        EGamePlatformUITransition::None);
    return true;
}

#endif
