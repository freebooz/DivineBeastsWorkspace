// 使用真实项目服务器装配验证竞技玩家接入统一操控，不授予网络准入或战斗资格。
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Server/DivineBeastsArenaServerProjectExtension.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaPlayerControlsInstallationTest,
    "DivineBeasts.Arena.Controls.ProjectAssembly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaPlayerControlsInstallationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    FDivineBeastsArenaServerProjectExtension Extension;
    FURL URL;
    URL.AddOption(*FString::Printf(TEXT("game=%s"), *AGamePlatformArenaGameMode::StaticClass()->GetPathName()));
    if (!TestTrue(TEXT("真实竞技GameMode创建"), World->SetGameMode(URL))) return false;
    World->InitializeActorsForPlay(URL);
    auto* Mode = World->GetAuthGameMode();
    auto* Controller = World->SpawnActor<APlayerController>(Mode->PlayerControllerClass);
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    Controller->SetPlayer(Player.Get());
    Controller->InitInputSystem();
    if (!TestNotNull(TEXT("引擎创建实际控制器输入组件"), Controller->InputComponent.Get())) return false;
    auto HasAxis = [&](FName Name)
    { return Controller->InputComponent->AxisBindings.ContainsByPredicate([&](const FInputAxisBinding& Axis) { return Axis.AxisName == Name && Axis.bConsumeInput; }); };
    TestTrue(TEXT("竞技场消费共同鼠标水平轴，不能落入Pawn默认视角"), HasAxis(TEXT("DBA.Turn")));
    TestTrue(TEXT("竞技场消费共同鼠标俯仰轴"), HasAxis(TEXT("DBA.Look")));
    TestTrue(TEXT("竞技场接入共同滚轮缩放"), HasAxis(TEXT("DBA.CameraZoom")));
    bool bObserve = false, bSteer = false;
    for (int32 Index = 0; Index < Controller->InputComponent->GetNumActionBindings(); ++Index)
    {
        const auto& Action = Controller->InputComponent->GetActionBinding(Index);
        bObserve |= Action.GetActionName() == TEXT("DBA.CameraObserve") && Action.KeyEvent == IE_Pressed;
        bSteer |= Action.GetActionName() == TEXT("DBA.CameraSteer") && Action.KeyEvent == IE_Pressed;
    }
    TestTrue(TEXT("竞技场支持左键观察"), bObserve);
    TestTrue(TEXT("竞技场支持右键转向"), bSteer);
    return true;
}
#endif
