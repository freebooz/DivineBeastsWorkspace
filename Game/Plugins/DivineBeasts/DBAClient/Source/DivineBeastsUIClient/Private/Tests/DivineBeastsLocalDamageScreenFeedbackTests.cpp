// 项目本地受击反馈自动化测试：核验仅当前受控玩家的权威生命伤害事件生成显示信号，不触碰Gameplay或蓝图资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Layers/DivineBeastsRootLayout.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLocalDamageScreenFeedbackTest,
    "DivineBeasts.UI.Combat.LocalDamageScreenFeedback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsLocalDamageScreenFeedbackTest::RunTest(const FString&)
{
    // 瞬态世界内使用真实Actor对象，覆盖本地玩家/其他玩家隔离；离开时销毁测试世界。
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("受击反馈瞬态世界"), World)) return false;
    AActor* Local = World->SpawnActor<AActor>();
    AActor* Other = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("本地玩家Actor"), Local) || !TestNotNull(TEXT("其他玩家Actor"), Other))
    { World->DestroyWorld(false); return false; }

    FGamePlatformCombatEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.EventType = EGamePlatformCombatEventType::Damage;
    Event.TargetActor = Local;
    Event.AppliedToHealth = 12.0f;
    float Fraction = 0.0f;
    TestTrue(TEXT("本地生命扣除应触发反馈"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Local, 100.0f, Fraction));
    TestTrue(TEXT("受击强度按最大生命归一化"), FMath::IsNearlyEqual(Fraction, 0.12f));

    TestFalse(TEXT("其他玩家受击不能触发本地屏幕红边"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Other, 100.0f, Fraction));
    TestEqual(TEXT("不合格信号应清空旧强度"), Fraction, 0.0f);
    Event.AppliedToHealth = 0.0f;
    Event.AppliedToShield = 12.0f;
    TestFalse(TEXT("只有护盾吸收不产生红边"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Local, 100.0f, Fraction));
    Event.AppliedToHealth = 12.0f;
    Event.EventType = EGamePlatformCombatEventType::Healing;
    TestFalse(TEXT("治疗不能产生红边"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Local, 100.0f, Fraction));
    Event.EventType = EGamePlatformCombatEventType::Damage;
    TestFalse(TEXT("未知最大生命不得凭空推断强度"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Local, 0.0f, Fraction));
    Event.EventId.Invalidate();
    TestFalse(TEXT("没有权威事件身份不能产生红边"),
        UDivineBeastsRootLayout::TryBuildLocalDamageFeedback(Event, Local, 100.0f, Fraction));

    World->DestroyWorld(false);
    return true;
}
#endif
