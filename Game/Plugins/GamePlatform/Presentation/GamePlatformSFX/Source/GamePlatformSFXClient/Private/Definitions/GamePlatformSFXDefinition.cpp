#include "Definitions/GamePlatformSFXDefinition.h"

FGamePlatformResult UGamePlatformSFXDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (Sound.IsNull())
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.SoundMissing"),
            TEXT("SFX Definition必须提供SoundBase软引用。"));
    }

    if (!FMath::IsFinite(VolumeMultiplier) || VolumeMultiplier < 0.0f || VolumeMultiplier > 4.0f ||
        !FMath::IsFinite(PitchMultiplier) || PitchMultiplier < 0.25f || PitchMultiplier > 4.0f ||
        !FMath::IsFinite(FadeInSeconds) || FadeInSeconds < 0.0f || FadeInSeconds > 10.0f ||
        !FMath::IsFinite(FadeOutSeconds) || FadeOutSeconds < 0.0f || FadeOutSeconds > 10.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("SFX.PlaybackRangeInvalid"),
            TEXT("SFX Definition的音量、音高或淡入淡出参数超出平台安全范围。"));
    }

    for (const FName AllowedName : AllowedFloatParameters)
    {
        if (AllowedName.IsNone())
        {
            return FGamePlatformResult::Failure(
                TEXT("SFX.ParameterNameInvalid"),
                TEXT("SFX浮点参数白名单不能包含None。"));
        }
    }

    for (const TPair<FName, float>& Pair : DefaultFloatParameters)
    {
        if (Pair.Key.IsNone() || !AllowedFloatParameters.Contains(Pair.Key) || !FMath::IsFinite(Pair.Value))
        {
            return FGamePlatformResult::Failure(
                TEXT("SFX.DefaultParameterInvalid"),
                TEXT("SFX默认浮点参数必须位于白名单内且数值有限。"));
        }
    }

    return FGamePlatformResult::Success();
}
