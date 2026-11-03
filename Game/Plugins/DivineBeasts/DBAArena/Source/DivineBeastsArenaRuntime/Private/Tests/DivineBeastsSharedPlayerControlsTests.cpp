#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Framework/DivineBeastsArenaPlayerController.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Gameplay/DivineBeastsPlayerControlsComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

// 真实两种控制器、真实输入绑定和Pawn运动。无桌面夹具仅将MouseButtonMask设为已通过视口捕获的前提；
// UI命中及捕获保护另由现有MenuFocus/CanBeginViewportDrag回归覆盖，不把该前提冒充实机鼠标事件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSharedPlayerControlsTest,
    "DivineBeasts.Arena.Controls.SharedMouseMovementAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSharedPlayerControlsTest::RunTest(const FString&)
{
    for (UClass* Class : {ADivineBeastsWorldPlayerController::StaticClass(), ADivineBeastsArenaPlayerController::StaticClass()})
    {
        TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
        ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
        World->InitializeActorsForPlay(FURL());
        auto* Controller = World->SpawnActor<APlayerController>(Class);
        TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine)); Controller->SetPlayer(Player.Get());
        auto* Pawn = World->SpawnActor<ACharacter>(); Controller->Possess(Pawn); Controller->InitInputSystem();
        auto* Boom = NewObject<USpringArmComponent>(Pawn); Pawn->AddInstanceComponent(Boom); Boom->RegisterComponent();
        Boom->TargetArmLength = 500.f;
        auto* Controls = Controller->FindComponentByClass<UDivineBeastsPlayerControlsComponent>();
        if (!TestNotNull(TEXT("两种真实控制器均组合同一操控类型"), Controls)) return false;
        Controller->SetControlRotation(FRotator::ZeroRotator); Controls->SetGameplayActive(true);
        TestTrue(TEXT("本地当前Pawn允许输入"), Controls->CanProcessWorldInput());
        auto Axis = [&](FName Name, float Value)
        {
            auto* Binding = Controller->InputComponent->AxisBindings.FindByPredicate([&](const FInputAxisBinding& Entry) { return Entry.AxisName == Name; });
            if (!TestNotNull(TEXT("真实原生轴已注册"), Binding)) return;
            Binding->AxisDelegate.Execute(Value);
        };
        const int32 AxisCount = Controller->InputComponent->AxisBindings.Num(); Controls->BindInput();
        TestEqual(TEXT("重复接收器通知不重复绑定"), Controller->InputComponent->AxisBindings.Num(), AxisCount);
        Axis(TEXT("DBA.Turn"),12.f);
        TestEqual(TEXT("未拖动时鼠标不转镜头"), Controls->ObservationRotation.Yaw,0.);
        // 已由桌面视口接纳LMB：相机水平/俯仰变化，角色不转。
        Controls->MouseButtonMask=1; Axis(TEXT("DBA.Turn"),12.f); Axis(TEXT("DBA.Look"),-7.f);
        TestEqual(TEXT("左键观察水平角"),Controls->ObservationRotation.Yaw,12.);
        TestEqual(TEXT("左键观察俯仰角"),Controls->ObservationRotation.Pitch,-22.);
        TestEqual(TEXT("左键不改变角色朝向"),Controller->GetControlRotation().Yaw,0.);
        Controls->MouseButtonMask=2; Axis(TEXT("DBA.Turn"),5.f); Axis(TEXT("DBA.Look"),3.f);
        TestEqual(TEXT("右键同步角色水平角"),Controller->GetControlRotation().Yaw,17.);
        TestEqual(TEXT("镜头俯仰不进入角色姿态"),Controller->GetControlRotation().Pitch,0.);
        Controls->MouseButtonMask=3; Axis(TEXT("DBA.MoveForward"),-1.f); Pawn->ConsumeMovementInputVector();
        Controls->ProcessInput(.1f,false);
        TestTrue(TEXT("双键前进优先于后退"),FVector::DotProduct(Pawn->ConsumeMovementInputVector(),FRotator(0,17.,0).Vector())>.99);
        Axis(TEXT("DBA.CameraZoom"),1.f); TestEqual(TEXT("两场景滚轮同向同距离"),Boom->TargetArmLength,450.f);
        Controller->SetIgnoreMoveInput(true); Controls->SetWorldGameplayInputSuspended(true);
        TestEqual(TEXT("菜单清除鼠标按住"),Controls->MouseButtonMask,static_cast<uint8>(0));
        TestTrue(TEXT("菜单恢复可见指针"),Controller->bShowMouseCursor);
        Axis(TEXT("DBA.Turn"),40.f); TestEqual(TEXT("菜单不穿透旋转"),Controls->ObservationRotation.Yaw,17.);
        Controls->SetWorldGameplayInputSuspended(false);
        TestTrue(TEXT("恢复后保留其它所有者移动锁"),Controller->IsMoveInputIgnored()); Controller->SetIgnoreMoveInput(false);
        Controls->MouseButtonMask=3; Controller->FlushPressedKeys();
        TestEqual(TEXT("真实失焦Flush清除双键"),Controls->MouseButtonMask,static_cast<uint8>(0));
        Controls->MouseButtonMask=2; Controls->SetTravelPending(true);
        TestFalse(TEXT("旅行门闩关闭操控"),Controls->CanProcessWorldInput());
        Controls->SetTravelPending(false); TestEqual(TEXT("旅行后不恢复旧拖动"),Controls->MouseButtonMask,static_cast<uint8>(0));
        Axis(TEXT("DBA.MoveForward"),0.f); Pawn->ConsumeMovementInputVector();
        Controls->ApplySemanticMoveLook(FVector2D(0,1),true,true); Controls->ProcessInput(.1f,false);
        TestTrue(TEXT("手柄与原生轴零值不互相覆盖"),Pawn->ConsumeMovementInputVector().Size()>.99);
        Controls->ProcessInput(.1f,false); TestTrue(TEXT("没有下一帧语义输入时不残留自动前进"),Pawn->ConsumeMovementInputVector().IsNearlyZero());
        Controls->SetSemanticInputOwned(true);
        Axis(TEXT("DBA.MoveForward"),1.f); Controls->ProcessInput(.1f,false);
        TestTrue(TEXT("Profile接管后旧原生键位不绕过重绑定"),Pawn->ConsumeMovementInputVector().IsNearlyZero());
        Controls->ApplySemanticMoveLook(FVector2D(0,.5),true,true); Axis(TEXT("DBA.MoveForward"),-1.f);
        Controls->ProcessInput(.1f,false);
        TestTrue(TEXT("重映射语义只消费一次且保留模拟幅度"),FMath::IsNearlyEqual(Pawn->ConsumeMovementInputVector().Size(),.5,.001));
        Controls->MouseButtonMask=2; const double BeforeSemantic=Controls->ObservationRotation.Yaw;
        Controls->ApplySemanticMoveLook(FVector2D(4,0),false,false); Axis(TEXT("DBA.Turn"),4.f);
        TestEqual(TEXT("原生与Enhanced同时到达也不叠加视角"),Controls->ObservationRotation.Yaw-BeforeSemantic,4.);
        Controls->SetSemanticInputOwned(true,false); Controls->MouseButtonMask=2;
        const double BeforePartial=Controls->ObservationRotation.Yaw; Axis(TEXT("DBA.Turn"),3.f);
        TestEqual(TEXT("只接管Move的Profile保留原生鼠标观察"),Controls->ObservationRotation.Yaw-BeforePartial,3.);
        Controls->SetSemanticInputOwned(false,true); Axis(TEXT("DBA.MoveForward"),1.f); Controls->ProcessInput(.1f,false);
        TestTrue(TEXT("只接管Look的Profile保留原生角色移动"),Pawn->ConsumeMovementInputVector().Size()>.99);
        Controls->SetSemanticInputOwned(false);
        Controls->MouseButtonMask=3; Controls->SetGameplayActive(false);
        TestFalse(TEXT("死亡/失活关闭操控"),Controls->CanProcessWorldInput());
        TestEqual(TEXT("死亡清除拖动"),Controls->MouseButtonMask,static_cast<uint8>(0));
        Controller->UnPossess(); Controls->SetGameplayActive(true);
        TestFalse(TEXT("没有当前拥有Pawn不能靠本地事实开启输入"),Controls->CanProcessWorldInput());
        if (Cast<ADivineBeastsArenaPlayerController>(Controller))
        {
            int32 ReceiverChanges=0;
            Controls->OnInputReceiverChanged.AddLambda([&ReceiverChanges] { ++ReceiverChanges; });
            Controller->OnRep_PlayerState();
            TestEqual(TEXT("迟到PlayerState必须触发就绪重采样"),ReceiverChanges,1);
            Controls->OnInputReceiverChanged.Clear();
        }
    }
    return true;
}
#endif
