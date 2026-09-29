#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Policy/GamePlatformDeviceSettingsPolicy.h"

#include <limits>

namespace
{
    FGamePlatformDeviceSettings MakeValidSettings()
    {
        FGamePlatformDeviceSettings Settings;
        Settings.Resolution = FIntPoint(1920, 1080);
        Settings.WindowMode =
            EGamePlatformWindowMode::WindowedFullscreen;
        Settings.bVSyncEnabled = true;
        Settings.FrameRateLimit = 120.0f;
        Settings.ViewDistanceQuality = 3;
        Settings.AntiAliasingQuality = 3;
        Settings.ShadowQuality = 2;
        Settings.GlobalIlluminationQuality = 2;
        Settings.ReflectionQuality = 2;
        Settings.PostProcessQuality = 3;
        Settings.TextureQuality = 3;
        Settings.EffectsQuality = 2;
        Settings.FoliageQuality = 2;
        Settings.ShadingQuality = 3;
        return Settings;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsValidPolicyTest,
    "GamePlatform.Settings.Policy.AcceptsValidDeviceSettings",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsValidPolicyTest::RunTest(
    const FString& Parameters)
{
    const FGamePlatformResult Result =
        FGamePlatformDeviceSettingsPolicy::Validate(
            MakeValidSettings());
    TestTrue(TEXT("合法设备设置应通过严格校验"), Result.IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsInvalidResolutionTest,
    "GamePlatform.Settings.Policy.RejectsInvalidResolution",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsInvalidResolutionTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformDeviceSettings Settings = MakeValidSettings();
    Settings.Resolution = FIntPoint(0, 1080);

    const FGamePlatformResult Result =
        FGamePlatformDeviceSettingsPolicy::Validate(Settings);
    TestFalse(TEXT("零宽度分辨率必须失败"), Result.IsSuccess());
    TestEqual(
        TEXT("错误码应稳定可检索"),
        Result.Code,
        FName(TEXT("SettingsResolutionInvalid")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsNonFiniteFrameLimitTest,
    "GamePlatform.Settings.Policy.RejectsNonFiniteFrameLimit",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsNonFiniteFrameLimitTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformDeviceSettings Settings = MakeValidSettings();
    Settings.FrameRateLimit =
        std::numeric_limits<float>::quiet_NaN();

    const FGamePlatformResult Result =
        FGamePlatformDeviceSettingsPolicy::Validate(Settings);
    TestFalse(TEXT("NaN帧率上限必须失败"), Result.IsSuccess());
    TestEqual(
        TEXT("错误码应稳定可检索"),
        Result.Code,
        FName(TEXT("SettingsFrameRateInvalid")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsInvalidQualityTest,
    "GamePlatform.Settings.Policy.RejectsOutOfRangeQuality",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsInvalidQualityTest::RunTest(
    const FString& Parameters)
{
    FGamePlatformDeviceSettings Settings = MakeValidSettings();
    Settings.ShadowQuality = 5;

    const FGamePlatformResult Result =
        FGamePlatformDeviceSettingsPolicy::Validate(Settings);
    TestFalse(TEXT("超过UE 0..4的画质等级必须失败"), Result.IsSuccess());
    TestEqual(
        TEXT("错误码应稳定可检索"),
        Result.Code,
        FName(TEXT("SettingsQualityInvalid")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformSettingsEquivalentToleranceTest,
    "GamePlatform.Settings.Policy.UsesStableFloatTolerance",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformSettingsEquivalentToleranceTest::RunTest(
    const FString& Parameters)
{
    const FGamePlatformDeviceSettings A = MakeValidSettings();
    FGamePlatformDeviceSettings B = A;
    B.FrameRateLimit += 0.005f;

    TestTrue(
        TEXT("微小浮点抖动不应制造设置变更事件"),
        FGamePlatformDeviceSettingsPolicy::AreEquivalent(A, B));

    B.FrameRateLimit += 1.0f;
    TestFalse(
        TEXT("真实帧率变化必须被识别"),
        FGamePlatformDeviceSettingsPolicy::AreEquivalent(A, B));
    return true;
}

#endif
