// SSLM狼怪物结构契约：无第三方Fab资源也可静态验证双端实体和平台中立AI/Combat组件。
// 注意此测试只验证装配与初始安全资格，不冒充Village真实碰撞、动画或联网验收。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/CapsuleComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageWolfAssemblyTest,
    "DivineBeasts.Characters.VillageWolf.ServerSafeAssembly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsVillageWolfAssemblyTest::RunTest(const FString&)
{
    const auto* Wolf = GetDefault<ADivineBeastsVillageWolf>();
    if (!TestNotNull(TEXT("必须由真正C++狼角色类构造默认对象"),Wolf)) return false;
    TestTrue(TEXT("狼复用唯一双端项目正式Character"),Wolf->IsA(ADivineBeastsCharacter::StaticClass()));
    TestNotNull(TEXT("狼持有平台服务器权威Combat组件"),Wolf->GetGamePlatformCombatComponent());
    TestNotNull(TEXT("狼使用平台GAS/ASC，不另建生命结算"),Wolf->GetGamePlatformAbilitySystemComponent());
    TestNotNull(TEXT("狼使用平台复制AI状态组件"),Wolf->GetWolfAIState());
    TestNotNull(TEXT("狼胶囊应具备真实四足怪物碰撞根"),Wolf->GetCapsuleComponent());
    TestFalse(TEXT("未有专服占有及真实Combat初始化前不能伤害玩家"),Wolf->CanSourceCombat());
    TestFalse(TEXT("未有专服初始化前不能接受旧伤害回执"),Wolf->CanReceiveCombat());
    TestFalse(TEXT("默认对象不能冒充已出生并拥有生命"),Wolf->IsWolfAlive());

    const auto* Player = GetDefault<ADivineBeastsCharacter>();
    if (TestNotNull(TEXT("正式玩家基础角色必须真实存在"),Player))
        TestNotNull(TEXT("正式角色必须装配平台AI目标身份组件"),Player->FindComponentByClass<UGamePlatformAITargetComponent>());
    return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
