// MainArena实例安全回收门禁的原生回归：不发真实HTTP、不调用Agones或移除玩家。
// 只验证低层结果终态、同Boot实例、真实控制器在场四个条件必须同时成立；
// 若任一前提失效，测试必须失败，防止SDK Shutdown过早终止仍有人的世界。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/GamePlatformArenaServerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformArenaSafeDrainGateTest,
    "GamePlatform.Arena.Server.SafeDrainAfterResultAndPlayerExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaSafeDrainGateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using FSubsystem = UGamePlatformArenaServerSubsystem;
    TestFalse(TEXT("后端结果未确认，即使零人也禁止回收"),
        FSubsystem::CanRequestSafeDrain(false, true, true, false));
    TestFalse(TEXT("仍在Ending/ResultPending阶段禁止回收"),
        FSubsystem::CanRequestSafeDrain(true, false, true, false));
    TestFalse(TEXT("被替换的GameServer实例身份禁止回收"),
        FSubsystem::CanRequestSafeDrain(true, true, false, false));
    TestFalse(TEXT("任一真实PlayerController仍存在禁止回收"),
        FSubsystem::CanRequestSafeDrain(true, true, true, true));
    TestTrue(TEXT("仅可信结果已提交、模式完成、身份一致及实际零人时允许启动Drain"),
        FSubsystem::CanRequestSafeDrain(true, true, true, false));
    return true;
}
#endif
