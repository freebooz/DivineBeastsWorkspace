#include "Feedback/MobaHitFeedbackPolicy.h"

FMobaHitFeedbackDecision FMobaHitFeedbackPolicy::Evaluate(
    const FMobaHitFeedbackInput& Input,
    const FGamePlatformHitFeedbackTuning& Tuning)
{
    FMobaHitFeedbackDecision Decision;
    if (Input.Contact == EMobaHitFeedbackContact::Missed)
    {
        // 挥空不产生接触反馈；未命中音和挥击动作由输入/技能动画通道独立管理。
        return Decision;
    }

    Decision.bHasContact = true;
    const bool bBlocked = Input.Contact == EMobaHitFeedbackContact::Blocked;
    int32 BaseFrames = 0;
    float BaseStrength = 0.6f;
    switch (Input.Contact)
    {
    case EMobaHitFeedbackContact::Heavy:
        BaseFrames = Tuning.HeavyHitstopFrames;
        BaseStrength = 1.0f;
        break;
    case EMobaHitFeedbackContact::Skill:
        BaseFrames = Tuning.SkillHitstopFrames;
        BaseStrength = 0.85f;
        break;
    case EMobaHitFeedbackContact::Blocked:
        BaseFrames = Tuning.BlockHitstopFrames;
        BaseStrength = 0.5f;
        break;
    case EMobaHitFeedbackContact::Light:
    default:
        BaseFrames = Tuning.LightHitstopFrames;
        break;
    }

    // 没有暴击表现：只有可信确认的非格挡破防事件可以增加局部视觉顿帧。
    // bGuardBroken（已确认破防）只是表现事实，不恢复失衡/韧性数值。
    const int32 ExtraFrames = !bBlocked && Input.bGuardBroken
        ? FMath::Clamp(Tuning.GuardBreakExtraFrames, 0, 3)
        : 0;
    const int32 Limit = FMath::Clamp(Tuning.MaxHitstopFrames, 0, 10);
    Decision.VisualHitstopFrames = FMath::Clamp(BaseFrames + ExtraFrames, 0, Limit);
    Decision.VisualHitstopSeconds = Decision.VisualHitstopFrames / 60.0f;

    // 连击只增强表现强度，不逐段延长停顿；格挡立即恢复为第一段强度。
    const int32 EffectiveStep = bBlocked ? 1 : FMath::Clamp(Input.ComboStep, 1, 32);
    const float ComboStrength = FMath::Clamp(
        1.0f + (EffectiveStep - 1) *
            FMath::Clamp(Tuning.ComboStrengthPerStep, 0.0f, 0.3f),
        1.0f, FMath::Clamp(Tuning.MaxComboStrength, 1.0f, 2.0f));
    const float BlockRatio = bBlocked
        ? FMath::Clamp(Tuning.BlockStrengthRatio, 0.0f, 1.0f) : 1.0f;
    const float LocalVictimRatio = Input.bLocalVictim
        ? FMath::Clamp(Tuning.LocalVictimStrengthRatio, 1.0f, 1.5f) : 1.0f;
    Decision.Strength = FMath::Clamp(
        BaseStrength * ComboStrength * BlockRatio * LocalVictimRatio, 0.0f, 2.0f);
    Decision.FlashSeconds = FMath::Clamp(Tuning.HitFlashSeconds, 0.0f, 0.05f);
    return Decision;
}
