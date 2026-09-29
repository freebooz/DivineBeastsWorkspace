#include "Policy/GamePlatformDeviceSettingsPolicy.h"

namespace
{
    bool IsQualityLevelValid(const int32 Value)
    {
        return Value >= 0 && Value <= 4;
    }
}

FGamePlatformResult FGamePlatformDeviceSettingsPolicy::Validate(
    const FGamePlatformDeviceSettings& Settings)
{
    // 只做平台安全边界校验；具体项目推荐档位由项目层数据决定。
    if (Settings.Resolution.X < 320 || Settings.Resolution.Y < 200 ||
        Settings.Resolution.X > 32768 || Settings.Resolution.Y > 32768)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsResolutionInvalid"),
            TEXT("分辨率超出平台安全范围（320x200 至 32768x32768）。"));
    }

    switch (Settings.WindowMode)
    {
    case EGamePlatformWindowMode::Windowed:
    case EGamePlatformWindowMode::WindowedFullscreen:
    case EGamePlatformWindowMode::Fullscreen:
        break;
    default:
        return FGamePlatformResult::Failure(
            TEXT("SettingsWindowModeInvalid"),
            TEXT("窗口模式不是受支持的稳定枚举值。"));
    }

    if (!FMath::IsFinite(Settings.FrameRateLimit) ||
        Settings.FrameRateLimit < 0.0f ||
        Settings.FrameRateLimit > 1000.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsFrameRateInvalid"),
            TEXT("帧率上限必须是0..1000之间的有限数，0表示不限制。"));
    }

    const int32 QualityLevels[] =
    {
        Settings.ViewDistanceQuality,
        Settings.AntiAliasingQuality,
        Settings.ShadowQuality,
        Settings.GlobalIlluminationQuality,
        Settings.ReflectionQuality,
        Settings.PostProcessQuality,
        Settings.TextureQuality,
        Settings.EffectsQuality,
        Settings.FoliageQuality,
        Settings.ShadingQuality
    };

    for (const int32 Quality : QualityLevels)
    {
        if (!IsQualityLevelValid(Quality))
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsQualityInvalid"),
                TEXT("画质等级必须位于UE原生0..4范围。"));
        }
    }

    return FGamePlatformResult::Success();
}

bool FGamePlatformDeviceSettingsPolicy::AreEquivalent(
    const FGamePlatformDeviceSettings& A,
    const FGamePlatformDeviceSettings& B)
{
    return A.Resolution == B.Resolution &&
        A.WindowMode == B.WindowMode &&
        A.bVSyncEnabled == B.bVSyncEnabled &&
        FMath::IsNearlyEqual(A.FrameRateLimit, B.FrameRateLimit, 0.01f) &&
        A.ViewDistanceQuality == B.ViewDistanceQuality &&
        A.AntiAliasingQuality == B.AntiAliasingQuality &&
        A.ShadowQuality == B.ShadowQuality &&
        A.GlobalIlluminationQuality == B.GlobalIlluminationQuality &&
        A.ReflectionQuality == B.ReflectionQuality &&
        A.PostProcessQuality == B.PostProcessQuality &&
        A.TextureQuality == B.TextureQuality &&
        A.EffectsQuality == B.EffectsQuality &&
        A.FoliageQuality == B.FoliageQuality &&
        A.ShadingQuality == B.ShadingQuality;
}
