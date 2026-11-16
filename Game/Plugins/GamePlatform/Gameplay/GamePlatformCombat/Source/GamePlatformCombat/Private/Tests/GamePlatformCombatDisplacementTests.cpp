#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Types/GamePlatformCombatTypes.h"

/** 不移动Actor的纯方向数值测试：仅验证平台中立推动/牵引的XY速度、长度及失败边界。
 * 未构建UE5.8目标时不能声称这份原生自动化实际通过。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatDisplacementGeometryTest,
    "GamePlatform.Combat.Displacement.V6.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatDisplacementGeometryTest::RunTest(const FString&)
{
    const FVector Epicenter(0.0, 0.0, 100.0);
    const FVector Target(100.0, 0.0, 100.0);
    FVector Output(9.0, 9.0, 9.0);
    TestTrue(TEXT("可计算服务器由内向外的推进方向"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Target, EGamePlatformDisplacementMode::PushAway, 400.0f, Output));
    TestTrue(TEXT("定向Push速度值符合源目标方向"),
        Output.Equals(FVector(400.0, 0.0, 0.0), 0.01));

    TestTrue(TEXT("可计算由外向内的牵引方向"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Target, EGamePlatformDisplacementMode::PullToward, 400.0f, Output));
    TestTrue(TEXT("Pull仅反转XY不抬升目标"),
        Output.Equals(FVector(-400.0, 0.0, 0.0), 0.01));

    TestFalse(TEXT("归零中心不构造随机推离方向"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Epicenter, EGamePlatformDisplacementMode::PushAway, 400.0f, Output));
    TestTrue(TEXT("失败时输出归零，不把旧速度带到后续攻击"), Output.IsNearlyZero());
    TestFalse(TEXT("超出速度上限拒绝"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Target, EGamePlatformDisplacementMode::PushAway, 801.0f, Output));
    TestFalse(TEXT("速度不足不得伪造生效"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Target, EGamePlatformDisplacementMode::PushAway, 99.0f, Output));
    TestFalse(TEXT("地面落差超160厘米时拒绝水平拖拉"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, FVector(100.0, 0.0, 400.0),
            EGamePlatformDisplacementMode::PullToward, 400.0f, Output));
    TestFalse(TEXT("未知枚举类型失败关闭"),
        UGamePlatformCombatComponent::BuildDisplacementVelocity(
            Epicenter, Target, static_cast<EGamePlatformDisplacementMode>(255), 400.0f, Output));
    return true;
}
#endif
