#if defined(GAMEPLATFORM_AI_NATIVE_TEST) && GAMEPLATFORM_AI_NATIVE_TEST
// 编辑地图/预览/命令行不能建立运行AI；真实Editor世界不Spawn由UE回归补证。
#include "../Perception/AIWorldPolicy.h"
#include <cstdlib>
#include <iostream>
static void Require(bool Value, const char* Message)
{ if (!Value) { std::cerr << Message << '\n'; std::exit(1); } }
int main()
{
    using namespace GamePlatformAIWorldPolicy;
    Require(!CanRun(false, true, false), "editor/preview world must never spawn authority controller");
    Require(!CanRun(true, true, true), "commandlet must never start runtime AI");
    Require(!CanRun(true, false, false), "client world must not run authority AI");
    Require(CanRun(true, true, false), "authority Game/PIE world may run AI");
}
#endif

// 空闲移动生产规则的UE回归；原生抽取验证只在Saved临时目录执行，不代替原生导航运行。
#if WITH_DEV_AUTOMATION_TESTS && !(defined(GAMEPLATFORM_AI_NATIVE_TEST) && GAMEPLATFORM_AI_NATIVE_TEST)
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Controllers/GamePlatformAIController.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Tags/GamePlatformAITags.h"

namespace
{
/** 比较同一组正常、取消、失败及边界输入；断言文本说明违约行为。 */
template <typename TDecide, typename TCheck>
void VerifyIdleMovement(TDecide&& Decide, TCheck&& Check)
{
    using EAction = decltype(Decide(0.0, 0.0f, 0.0f, false, false, false, false));
    // 巡逻半径5米、归家容差80厘米：巡逻不能在离家160厘米时被当成脱战归家。
    Check(Decide(300.0 * 300.0, 500.0f, 80.0f, true, false, true, true) == EAction::ContinueMove,
        "active patrol must continue beyond return-home tolerance");
    Check(Decide(350.0 * 350.0, 500.0f, 80.0f, true, false, false, true) == EAction::SelectPatrolGoal,
        "completed patrol inside patrol radius selects another waypoint");
    // 可达路径允许绕障暂时离开采样圆；已经提交的巡逻路径不每次决策被重置。
    Check(Decide(700.0 * 700.0, 500.0f, 80.0f, true, false, true, true) == EAction::ContinueMove,
        "navigation detour must not restart an active patrol path");
    Check(Decide(501.0 * 501.0, 500.0f, 80.0f, true, false, false, true) == EAction::ReturnHome,
        "idle displaced patrol outside sampling radius returns home");
    Check(Decide(300.0 * 300.0, 500.0f, 80.0f, false, false, true, true) == EAction::ReturnHome,
        "lost-target movement cannot be kept as an idle patrol");
    Check(Decide(100.0 * 100.0, 500.0f, 80.0f, false, false, true, true) == EAction::SelectPatrolGoal,
        "lost target near home can immediately start a patrol");
    Check(Decide(300.0 * 300.0, 500.0f, 80.0f, false, true, true, true) == EAction::ContinueMove,
        "return-home request must not restart every decision");
    Check(Decide(100.0 * 100.0, 500.0f, 80.0f, false, true, true, true) == EAction::ContinueMove,
        "return-home path continues until actual path completion");
    Check(Decide(50.0 * 50.0, 500.0f, 80.0f, false, true, false, true) == EAction::SelectPatrolGoal,
        "completed return-home resumes patrol");
    Check(Decide(300.0 * 300.0, 500.0f, 80.0f, false, true, false, false) == EAction::Wait,
        "failed return-home movement respects retry backoff");
    Check(Decide(0.0, 500.0f, 80.0f, false, false, false, false) == EAction::Wait,
        "failed navigation selection respects retry backoff");
    Check(Decide(0.0, 500.0f, 80.0f, false, false, false, true) == EAction::SelectPatrolGoal,
        "initial no-target snapshot starts patrol");
    Check(Decide(0.0, 0.0f, 80.0f, false, false, false, true) == EAction::Wait,
        "zero patrol radius disables wandering without navigation error");
    Check(Decide(300.0 * 300.0, 0.0f, 80.0f, false, false, false, true) == EAction::ReturnHome,
        "disabled patrol still returns after losing target far from home");
    Check(Decide(500.0 * 500.0, 500.0f, 80.0f, true, false, false, true) == EAction::SelectPatrolGoal,
        "exact patrol radius remains inside patrol domain");
    Check(Decide(160.0 * 160.0, 500.0f, 80.0f, false, false, false, true) == EAction::SelectPatrolGoal,
        "return-home threshold preserves existing inclusive boundary");
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIIdleMovementPolicyTest, "GamePlatform.AI.Movement.IdlePatrolLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAIIdleMovementPolicyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    VerifyIdleMovement(&AGamePlatformAIController::DecideIdleMovement, [this](bool bPassed, const char* Message) { TestTrue(UTF8_TO_TCHAR(Message), bPassed); });

    // 真实权威Controller调用回归：世界故意没有导航和可移动Pawn，验证失败不会伪报Moving。
    // 只为移动阶段注入瞬态定义，不声称通过Data租约/Brain加载，也不生成任何正式原生资产。
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("真实Game测试世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("不可移动测试Pawn"), Pawn)) return false;
    auto* State = NewObject<UGamePlatformAIStateComponent>(Pawn);
    Pawn->AddInstanceComponent(State);
    State->RegisterComponent();
    auto* Controller = World->SpawnActor<AGamePlatformAIController>();
    if (!TestNotNull(TEXT("真实平台AI控制器"), Controller)) return false;
    Controller->Possess(Pawn);
    auto* Definition = NewObject<UGamePlatformAIDefinition>(Controller);
    Definition->BrainType = EGamePlatformAIBrainType::None;
    Definition->HomePolicy.PatrolRadius = 500.0f;
    Definition->HomePolicy.ReturnHomeAcceptanceRadius = 80.0f;
    Controller->ActiveDefinition = Definition;
    Controller->bBrainReady = true;
    State->InitializeServerState(TEXT("Test.IdleMovement"), 1);
    Controller->ForceDecisionUpdate();
    TestEqual(TEXT("首次无目标即实际请求巡逻导航"), Controller->GetLastAIError(), EGamePlatformAIError::NavigationUnavailable);
    TestEqual(TEXT("无导航明确显示Idle"), State->GetSnapshot().PublicState, EGamePlatformAIPublicState::Idle);
    const double FirstRetryTime = Controller->NextPatrolSelectionTime;
    TestTrue(TEXT("首次失败建立有限退避"), FirstRetryTime > World->GetTimeSeconds());
    Controller->ForceDecisionUpdate();
    TestEqual(TEXT("退避内不反复查询导航或延长下一次尝试"), Controller->NextPatrolSelectionTime, FirstRetryTime);

    // 将既有决策时钟的重试时间推进到到期，实际入口必须再次查询，不能被初次失败永久禁用。
    Controller->NextPatrolSelectionTime = 0.0;
    Controller->LastError = EGamePlatformAIError::None;
    Controller->ForceDecisionUpdate();
    TestEqual(TEXT("导航失败后到期会重试"), Controller->GetLastAIError(), EGamePlatformAIError::NavigationUnavailable);

    Controller->HomeLocation = Pawn->GetActorLocation() + FVector(1000.0, 0.0, 0.0);
    Controller->RequestReturnHome();
    TestEqual(TEXT("实际MoveTo被拒绝时保持Idle"), State->GetSnapshot().PublicState, EGamePlatformAIPublicState::Idle);
    TestEqual(TEXT("实际MoveTo失败保留导航诊断"), Controller->GetLastAIError(), EGamePlatformAIError::NavigationUnavailable);

    // 真实Controller死亡入口与后继决策：已有死亡停机规则不能因空闲巡逻恢复而失效。
    FGamePlatformCombatEvent Death;
    Death.EventType = EGamePlatformCombatEventType::Death;
    Death.TargetActor = Pawn;
    Controller->HandleCombatEvent(Death);
    Controller->NextPatrolSelectionTime = 0.0;
    Controller->ForceDecisionUpdate();
    TestEqual(TEXT("死亡后不会恢复巡逻"), State->GetSnapshot().PublicState, EGamePlatformAIPublicState::Dead);
    TestEqual(TEXT("死亡后未提交新导航请求"), Controller->NextPatrolSelectionTime, 0.0);

    Controller->NextPatrolSelectionTime = 1000.0;
    Controller->UnPossess();
    Controller->Possess(Pawn);
    TestEqual(TEXT("重新占有清除旧巡逻退避"), Controller->NextPatrolSelectionTime, 0.0);
    return true;
}
#endif
