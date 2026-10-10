#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Feedback/DivineBeastsHitFeedbackGrantPolicy.h"

/**
 * 验证目标：同一英雄、代次、技能集合重复接收时不重新租约化；
 * 授权撤销、英雄/代次变更或技能添加均应释放旧资源并重新预热。
 * 不模拟真实GAS服务器，也不将此测试当作角色联机重生验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsHitFeedbackGrantPolicyTest,
    "DivineBeasts.Arena.CombatFeedback.GrantPolicy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHitFeedbackGrantPolicyTest::RunTest(const FString&)
{
    using namespace DivineBeastsHitFeedbackGrantPolicy;

    const FPrimaryAssetId A(
        FPrimaryAssetType(TEXT("GamePlatformDefinition")), FName(TEXT("Profile.Light")));
    const FPrimaryAssetId B(
        FPrimaryAssetType(TEXT("GamePlatformDefinition")), FName(TEXT("Profile.Skill")));
    const FPrimaryAssetId C(
        FPrimaryAssetType(TEXT("GamePlatformDefinition")), FName(TEXT("Profile.Other")));

    TSet<FPrimaryAssetId> Initial{A, B};
    TSet<FPrimaryAssetId> Reordered{B, A};
    TSet<FPrimaryAssetId> Added{A, B, C};
    TSet<FPrimaryAssetId> Removed{A};

    TestFalse(TEXT("相同授权身份和技能集合不触发重复资源申请"),
        RequiresRefresh(Initial, TEXT("Hero.Ox"), 7, Reordered, TEXT("Hero.Ox"), 7));
    TestTrue(TEXT("新增技能需预热新Profile"),
        RequiresRefresh(Initial, TEXT("Hero.Ox"), 7, Added, TEXT("Hero.Ox"), 7));
    TestTrue(TEXT("服务端撤销技能应释放旧租约"),
        RequiresRefresh(Initial, TEXT("Hero.Ox"), 7, Removed, TEXT("Hero.Ox"), 7));
    TestTrue(TEXT("相同Profile但英雄发生变化也须重新绑定"),
        RequiresRefresh(Initial, TEXT("Hero.Ox"), 7, Reordered, TEXT("Hero.Tiger"), 7));
    TestTrue(TEXT("相同英雄和Profile但Avatar代次变化必须重新绑定"),
        RequiresRefresh(Initial, TEXT("Hero.Ox"), 7, Reordered, TEXT("Hero.Ox"), 8));
    TestTrue(TEXT("首次授予有效技能要创建缓存"),
        RequiresRefresh({}, NAME_None, 0, Initial, TEXT("Hero.Ox"), 1));
    TestFalse(TEXT("空缓存与相同空授权不会重复改变"),
        RequiresRefresh({}, NAME_None, 0, {}, NAME_None, 0));
    return true;
}
#endif
