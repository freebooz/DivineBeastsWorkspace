#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"

#include <limits>

/**
 * 只验证项目客户端浮字DTO转换为平台UI请求的纯数据逻辑；
 * 不生成Widget、不模拟服务端命中，也不将测试源码冒充可视化验收。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsCombatUIFeedbackTest,
    "DivineBeasts.UI.Combat.FeedbackMapping",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCombatUIFeedbackTest::RunTest(const FString&)
{
    FDivineBeastsCombatFeedbackInput Input;
    Input.EventId = FGuid::NewGuid();
    Input.TargetVisualKey = TEXT("Hero_Test_Target");
    Input.WorldLocation = FVector(100.0, 200.0, 300.0);
    Input.Magnitude = 35.0;

    FGamePlatformUIFeedbackRequest DamageRequest;
    TestTrue(TEXT("生命伤害转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, DamageRequest));

    FGamePlatformUIFeedbackRequest ShieldRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::ShieldDamage;
    Input.Magnitude = 12.0;
    TestTrue(TEXT("护盾吸收转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, ShieldRequest));
    TestNotEqual(TEXT("生命伤害与护盾吸收不能使用同一合并键"),
        DamageRequest.MergeKey, ShieldRequest.MergeKey);

    FGamePlatformUIFeedbackRequest HealingRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::Healing;
    Input.Magnitude = 23.0;
    TestTrue(TEXT("治疗转换"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, HealingRequest));
    TestNotEqual(TEXT("治疗与伤害不能合并"),
        DamageRequest.MergeKey, HealingRequest.MergeKey);
    TestNotEqual(TEXT("治疗与护盾不能合并"),
        HealingRequest.MergeKey, ShieldRequest.MergeKey);

    FGamePlatformUIFeedbackRequest DeathRequest;
    Input.Kind = EDivineBeastsCombatFeedbackKind::Death;
    Input.Magnitude = 0.0;
    TestTrue(TEXT("死亡为独立非数值提示"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, DeathRequest));
    TestTrue(TEXT("死亡提示优先级应高于普通伤害"),
        DeathRequest.Priority > DamageRequest.Priority);

    Input.Kind = EDivineBeastsCombatFeedbackKind::Damage;
    Input.Magnitude = std::numeric_limits<double>::quiet_NaN();
    FGamePlatformUIFeedbackRequest InvalidRequest;
    TestFalse(TEXT("拒绝NaN数值"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, InvalidRequest));

    Input.Magnitude = 1.0e30;
    FGamePlatformUIFeedbackRequest HugeRequest;
    TestTrue(TEXT("过大数值可安全裁剪为UI格式化区间"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, HugeRequest));
    TestEqual(TEXT("显示数值不得溢出int32"),
        HugeRequest.NumericValue, 1000000000.0);

    Input.WorldLocation.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("拒绝非有限世界投影坐标"),
        UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
            Input, InvalidRequest));

    return true;
}
#endif
