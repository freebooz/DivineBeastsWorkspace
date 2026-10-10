#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Feedback/GamePlatformCombatFeedbackDedupePolicy.h"

/**
 * 只检查可选表现缓存的键语义和有界生命周期，不依赖真实UWorld、
 * GameplayCue、服务器RPC、Niagara和声音资源，不能冒称联机网络已验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatFeedbackDedupeTest,
    "GamePlatform.Combat.Feedback.Dedupe",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatFeedbackDedupeTest::RunTest(const FString&)
{
    using namespace GamePlatformCombatFeedbackDedupe;
    FCache Cache;
    const FGuid A = FGuid::NewGuid();
    const FGuid B = FGuid::NewGuid();
    const FGuid C = FGuid::NewGuid();

    TestFalse(TEXT("拒绝无效Guid"),
        Cache.TryRemember(FGuid(), EGamePlatformCombatEventType::Damage, 2));
    TestTrue(TEXT("第一次Damage可以进入客户端表现"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Damage, 2));
    TestFalse(TEXT("重复Damage必须去重"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Damage, 2));
    TestTrue(TEXT("同一Guid的Death是独立事实，不得被Damage吞掉"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Death, 2));
    TestFalse(TEXT("重复Death仍然去重"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Death, 2));

    TestTrue(TEXT("允许第二个Guid"),
        Cache.TryRemember(B, EGamePlatformCombatEventType::Damage, 2));
    TestTrue(TEXT("第三个Guid可挤出最旧事件"),
        Cache.TryRemember(C, EGamePlatformCombatEventType::Damage, 2));
    TestEqual(TEXT("环形缓存严格有界"), Cache.NumUniqueEvents(), 2);
    TestTrue(TEXT("被挤出的Guid允许以后再次产生新的瞬时反馈"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Damage, 2));
    TestEqual(TEXT("缓存容量始终为2"), Cache.NumUniqueEvents(), 2);

    TestFalse(TEXT("拒绝非法事件类型索引"),
        Cache.TryRemember(B, static_cast<EGamePlatformCombatEventType>(255), 2));
    Cache.Reset();
    TestEqual(TEXT("当前World关闭必须释放缓存"), Cache.NumUniqueEvents(), 0);
    TestTrue(TEXT("World重建后新的事实可以被接受"),
        Cache.TryRemember(A, EGamePlatformCombatEventType::Damage, 2));
    return true;
}
#endif
