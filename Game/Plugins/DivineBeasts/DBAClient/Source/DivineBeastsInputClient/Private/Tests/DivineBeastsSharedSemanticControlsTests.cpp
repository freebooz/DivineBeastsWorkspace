#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Subsystems/DivineBeastsInputClientSubsystem.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Gameplay/DivineBeastsPlayerControlsComponent.h"
#include "Services/GamePlatformInputServices.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSharedSemanticControlsTest,
    "DivineBeasts.Input.SharedControls.SemanticOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSharedSemanticControlsTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT {Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
    World->InitializeActorsForPlay(FURL());
    auto* Controller=World->SpawnActor<ADivineBeastsWorldPlayerController>();
    TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    Viewport->Init(GEngine->GetWorldContextFromWorldChecked(World),Instance.Get(),false);
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));Player->ViewportClient=Viewport.Get();
    ON_SCOPE_EXIT {Player->ViewportClient=nullptr;};
    Controller->SetPlayer(Player.Get());
    auto* Pawn=World->SpawnActor<ACharacter>();Controller->Possess(Pawn);Controller->InitInputSystem();
    auto* Controls=Controller->GetPlayerControls();Controls->SetGameplayActive(true);
    TStrongObjectPtr<UDivineBeastsInputClientSubsystem> Input(NewObject<UDivineBeastsInputClientSubsystem>(Player.Get()));
    Input->CachedController=Controller;Input->CachedPawn=Pawn;
    FGamePlatformInputEvent Event;Event.Phase=ETriggerEvent::Triggered;Event.Value=FInputActionValue(FVector2D(0,1));
    Event.Unit=EGamePlatformInputUnit::NormalizedAxis;
    Event.SemanticId.Tag=GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move);
    Event.DeviceFamily=EGamePlatformInputDeviceFamily::KeyboardMouse;
    Input->HandleMoveLookInput(Event);Controls->ProcessInput(.1f,false);
    TestTrue(TEXT("键鼠Profile移动语义不能因设备提示被丢弃"),Pawn->ConsumeMovementInputVector().Size()>.99);
    Event.SemanticId.Tag=GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookRate);
    Event.Value=FInputActionValue(FVector2D(90,0));Event.Unit=EGamePlatformInputUnit::DegreesPerSecond;
    World->Tick(LEVELTICK_TimeOnly,.1f);
    if (!TestTrue(TEXT("速率消费使用真实LocalPlayer视口世界"),Input->GetWorld()==World && World->GetDeltaSeconds()>0.f)) return false;
    const double BeforeRate=Controller->GetControlRotation().Yaw;Input->HandleMoveLookInput(Event);
    TestTrue(TEXT("LookRate按真实单位一次换算，不依赖最近设备提示"),FMath::IsNearlyEqual(Controller->GetControlRotation().Yaw-BeforeRate,90.*World->GetDeltaSeconds(),.001));
    Event.SemanticId.Tag=GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookDelta);
    Event.Unit=EGamePlatformInputUnit::DegreesDelta;Event.DeviceFamily=EGamePlatformInputDeviceFamily::Gamepad;
    const double BeforeMouse=Controller->GetControlRotation().Yaw;Input->HandleMoveLookInput(Event);
    TestEqual(TEXT("旧设备提示不能使鼠标无拖动旋转"),Controller->GetControlRotation().Yaw,BeforeMouse);
    return true;
}
#endif
