#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Feedback/GamePlatformLocalHitstopSubsystem.h"

/**
 * 验证目标：客户端Profile顿帧参考帧与测试控制台覆盖值必须确定性、有限且可关闭。
 * 不需要实际UWorld、Mesh或本地玩家，不修改服务器时钟。
 * 该测试仅覆盖0/3/6参考帧参数计算，不冒称已测试动画、根运动或输入回放。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformLocalHitstopTuningTest,
    "GamePlatform.Animation.Hitstop.Tuning",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformLocalHitstopTuningTest::RunTest(const FString&)
{
    auto Frames = [](int32 Profile, int32 Override)
    {
        return UGamePlatformLocalHitstopSubsystem::ResolveVisualHitstopFrames(
            Profile, Override);
    };

    TestEqual(TEXT("默认采用数据资产轻击3帧"), Frames(3, -1), 3);
    TestEqual(TEXT("默认采用数据资产重击6帧"), Frames(6, -1), 6);
    TestEqual(TEXT("本地0帧覆盖完全关闭顿帧"), Frames(6, 0), 0);
    TestEqual(TEXT("本地3帧独立覆盖重击配置"), Frames(6, 3), 3);
    TestEqual(TEXT("本地6帧独立覆盖轻击配置"), Frames(3, 6), 6);
    TestEqual(TEXT("非法负配置裁剪为零"), Frames(-12, -1), 0);
    TestEqual(TEXT("溢出帧数裁剪到10帧上限"), Frames(24, -1), 10);
    TestEqual(TEXT("本地覆盖也受10帧上限"), Frames(2, 50), 10);
    TestEqual(TEXT("关闭配置保留0帧"), Frames(0, -1), 0);
    return true;
}
#endif
