#if WITH_DEV_AUTOMATION_TESTS
// 游戏世界中先BeginPlay后Possess的行为回归；检查引擎拥有事件启动/停止定时采样，不使用业务Tick。
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
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
    APawn* Pawn = World->SpawnActor<APawn>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    UGamePlatformInteractorComponent* Interactor = NewObject<UGamePlatformInteractorComponent>(Pawn);
    Pawn->AddInstanceComponent(Interactor); Interactor->RegisterComponent(); Interactor->BeginPlay();
    TestFalse(TEXT("未拥有前不运行焦点采样"), Interactor->IsLocalFocusSamplingActive());
    Controller->Possess(Pawn);
    TestTrue(TEXT("延迟本地Possess事件立即启动集中采样"), Interactor->IsLocalFocusSamplingActive());
    Controller->UnPossess();
    TestFalse(TEXT("失去拥有关系时停止采样"), Interactor->IsLocalFocusSamplingActive());
    Interactor->EndPlay(EEndPlayReason::Destroyed); World->DestroyWorld(false);
    return true;
}
#endif
