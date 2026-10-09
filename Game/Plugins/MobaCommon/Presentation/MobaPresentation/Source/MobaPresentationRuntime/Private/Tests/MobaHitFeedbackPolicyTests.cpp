#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Feedback/MobaHitFeedbackPolicy.h"

/**
 * 验证目标：已取消暴击后，反馈策略仍对0/3/6帧、确认破防、格挡、挥空、连击与上限保持确定性。
 * 前置条件：模块加载，使用默认配置，不加载具体英雄/音效/Niagara资产。
 * 失败意义：战斗反馈可能误触发、无限累积顿帧或混淆60Hz参考时钟。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaHitFeedbackPolicyTest,
    "Moba.Presentation.HitFeedback.Policy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaHitFeedbackPolicyTest::RunTest(const FString&)
{
    FGamePlatformHitFeedbackTuning Tuning;
    FMobaHitFeedbackInput Input;
    Input.Contact = EMobaHitFeedbackContact::Light;

    Tuning.LightHitstopFrames = 0;
    TestEqual(TEXT("0帧关闭局部停顿"),
        FMobaHitFeedbackPolicy::Evaluate(Input, Tuning).VisualHitstopFrames, 0);
    Tuning.LightHitstopFrames = 3;
    TestEqual(TEXT("3帧轻击"),
        FMobaHitFeedbackPolicy::Evaluate(Input, Tuning).VisualHitstopFrames, 3);

    Input.Contact = EMobaHitFeedbackContact::Heavy;
    Tuning.HeavyHitstopFrames = 6;
    const FMobaHitFeedbackDecision Heavy = FMobaHitFeedbackPolicy::Evaluate(Input, Tuning);
    TestEqual(TEXT("6帧重击"), Heavy.VisualHitstopFrames, 6);
    TestTrue(TEXT("采用60Hz换算秒数"),
        FMath::IsNearlyEqual(Heavy.VisualHitstopSeconds, 0.1f));

    Input.bGuardBroken = true;
    Tuning.MaxHitstopFrames = 8;
    TestEqual(TEXT("确认破防额外顿帧不超过8帧"),
        FMobaHitFeedbackPolicy::Evaluate(Input, Tuning).VisualHitstopFrames, 8);

    Input.Contact = EMobaHitFeedbackContact::Blocked;
    Input.ComboStep = 20;
    const FMobaHitFeedbackDecision Blocked = FMobaHitFeedbackPolicy::Evaluate(Input, Tuning);
    TestEqual(TEXT("格挡不继承破防顿帧"), Blocked.VisualHitstopFrames, 1);
    TestTrue(TEXT("格挡立即降低反馈强度"), Blocked.Strength < Heavy.Strength);

    Input.Contact = EMobaHitFeedbackContact::Missed;
    const FMobaHitFeedbackDecision Missed = FMobaHitFeedbackPolicy::Evaluate(Input, Tuning);
    TestFalse(TEXT("挥空不产生接触"), Missed.bHasContact);
    TestEqual(TEXT("挥空零顿帧"), Missed.VisualHitstopFrames, 0);

    Input.Contact = EMobaHitFeedbackContact::Light;
    Input.bGuardBroken = false;
    Input.ComboStep = 1;
    const FMobaHitFeedbackDecision First = FMobaHitFeedbackPolicy::Evaluate(Input, Tuning);
    Input.ComboStep = 30;
    const FMobaHitFeedbackDecision Combo = FMobaHitFeedbackPolicy::Evaluate(Input, Tuning);
    TestTrue(TEXT("连击增强反馈"), Combo.Strength > First.Strength);
    TestEqual(TEXT("连击不会加长顿帧"), Combo.VisualHitstopFrames, First.VisualHitstopFrames);
    return true;
}
#endif
