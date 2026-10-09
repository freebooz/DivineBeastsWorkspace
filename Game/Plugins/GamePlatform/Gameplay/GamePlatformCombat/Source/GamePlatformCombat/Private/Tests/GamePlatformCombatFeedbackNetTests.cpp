#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformCombatFeedbackNetEvent.h"

/**
 * 验证目标：只有合法的服务器确认命中事实能够进入可选网络表现通道。
 * 本测试只检查数据契约边界，不宣称已经通过真实RPC、NetRelevancy或多人联机。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatFeedbackNetTest,
    "GamePlatform.Combat.Feedback.NetworkContract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatFeedbackNetTest::RunTest(const FString&)
{
    FGamePlatformCombatFeedbackNetEvent Event;
    TestFalse(TEXT("无Guid事件必须无效"), Event.IsSafeForCosmetics());

    Event.EventId = FGuid::NewGuid();
    Event.TargetAvatarGeneration = 2;
    Event.WorldContextGeneration = 3;
    Event.AppliedMagnitude = 42.0f;
    Event.AppliedToShield = 12.0f;
    Event.EventType = EGamePlatformCombatEventType::Damage;
    TestTrue(TEXT("合法命中只允许用于视觉表现"), Event.IsSafeForCosmetics());

    Event.AppliedToShield = -1.0f;
    TestFalse(TEXT("负护盾吸收拒绝"), Event.IsSafeForCosmetics());
    Event.AppliedToShield = 0.0f;

    Event.AppliedMagnitude = -0.01f;
    TestFalse(TEXT("负伤害拒绝"), Event.IsSafeForCosmetics());
    Event.AppliedMagnitude = 42.0f;

    Event.EventType = EGamePlatformCombatEventType::ControlRemoved;
    TestFalse(TEXT("旧控制移除不触发命中粒子"), Event.IsSafeForCosmetics());
    Event.EventType = EGamePlatformCombatEventType::Damage;

    Event.TargetAvatarGeneration = 0;
    TestFalse(TEXT("缺少目标AvatarGeneration拒绝"), Event.IsSafeForCosmetics());
    return true;
}
#endif
