#include "Feedback/GamePlatformHitFeedbackProfile.h"

FGamePlatformResult UGamePlatformHitFeedbackProfile::ValidateDefinition() const
{
    // 主资产ID、结构版本、内容修订与必需依赖统一由GamePlatformData验证。
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    const auto Within = [](float Value, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
    };

    if (Tuning.LightHitstopFrames < 0 || Tuning.LightHitstopFrames > 10 ||
        Tuning.HeavyHitstopFrames < 0 || Tuning.HeavyHitstopFrames > 10 ||
        Tuning.SkillHitstopFrames < 0 || Tuning.SkillHitstopFrames > 10 ||
        Tuning.BlockHitstopFrames < 0 || Tuning.BlockHitstopFrames > 10 ||
        Tuning.GuardBreakExtraFrames < 0 || Tuning.GuardBreakExtraFrames > 3 ||
        Tuning.MaxHitstopFrames < 0 || Tuning.MaxHitstopFrames > 10 ||
        !Within(Tuning.HitFlashSeconds, 0.0f, 0.05f) ||
        !Within(Tuning.CameraStrength, 0.0f, 2.0f) ||
        !Within(Tuning.AudioStrength, 0.0f, 2.0f) ||
        !Within(Tuning.VFXStrength, 0.0f, 2.0f) ||
        !Within(Tuning.ComboStrengthPerStep, 0.0f, 0.3f) ||
        !Within(Tuning.MaxComboStrength, 1.0f, 2.0f) ||
        !Within(Tuning.BlockStrengthRatio, 0.0f, 1.0f) ||
        !Within(Tuning.LocalVictimStrengthRatio, 1.0f, 1.5f))
    {
        return FGamePlatformResult::Failure(
            TEXT("InvalidHitFeedbackTuning"),
            TEXT("命中反馈参数包含非法区间或非有限数字，请检查Profile。"));
    }

    // 不加载CameraShake/Overlay材质；客户端租约只需请求Client Bundle。
    return FGamePlatformResult::Success();
}
