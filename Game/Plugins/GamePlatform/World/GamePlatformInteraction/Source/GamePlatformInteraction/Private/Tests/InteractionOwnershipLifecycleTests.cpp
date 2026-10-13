#if WITH_DEV_AUTOMATION_TESTS
// 真UE5.8游戏世界先BeginPlay后Possess行为回归：交互式本地焦点定时采样；
// 命令行/无本地玩家宿主必须保持静默，防止编辑器自动化进程意外长期创建采样计时器。
#include "Misc/AutomationTest.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionDelayedPossessTest,
    "GamePlatform.Interaction.Lifecycle.DelayedPossess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInteractionDelayedPossessTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    APawn* Pawn = World->SpawnActor<APawn>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    UGamePlatformInteractorComponent* Interactor = NewObject<UGamePlatformInteractorComponent>(Pawn);
    Pawn->AddInstanceComponent(Interactor); Interactor->RegisterComponent(); Interactor->BeginPlay();
    TestFalse(TEXT("未拥有前不运行焦点采样"), Interactor->IsLocalFocusSamplingActive());
    Controller->Possess(Pawn);
    // 裸临时World没有ULocalPlayer（本地玩家），独立APlayerController即使Possess也未必
    // 满足IsLocalController；不能把这个编辑器夹具当作真实本地客户端资格。
    // 本用例对真实引擎资格分别验证启停；有本地玩家的PIE正例仍须独立验证。
    const bool bHasLocalPlayer = Controller->IsLocalController();
    const bool bShouldSample = bHasLocalPlayer && !IsRunningCommandlet();
    TestEqual(TEXT("仅真实本地控制器并且不是Commandlet才能启动焦点采样"),
        Interactor->IsLocalFocusSamplingActive(), bShouldSample);
    Controller->UnPossess();
    TestFalse(TEXT("失去拥有关系时停止采样"), Interactor->IsLocalFocusSamplingActive());
    Interactor->EndPlay(EEndPlayReason::Destroyed); World->DestroyWorld(false);
    return true;
}
#endif
