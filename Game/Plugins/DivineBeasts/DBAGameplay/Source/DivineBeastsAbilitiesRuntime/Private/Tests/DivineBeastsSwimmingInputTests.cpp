// 游泳仍使用唯一原生输入链；验证地面、泳潜、视线与失控资格，不发坐标RPC。
#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSwimmingInputTests,
    "DivineBeasts.Water.SwimmingInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSwimmingInputTests::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    auto* Character = World->SpawnActor<ADivineBeastsGameplayCharacter>();
    Controller->Possess(Character);
    auto* Input = NewObject<UInputComponent>(Character);
    Character->SetupPlayerInputComponent(Input);
    const auto Axis = [&](FName Name, float Value)
    {
        for (auto& Binding : Input->AxisBindings)
            if (Binding.AxisName == Name) { Binding.AxisDelegate.Execute(Value); return true; }
        return false;
    };
    auto* Movement = Character->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Walking);
    TestTrue(TEXT("主线升降轴已经绑定"), Axis(TEXT("DBA.SwimVertical"), 1.f));
    TestTrue(TEXT("地面不能受水下升降轴影响"), Character->ConsumeMovementInputVector().IsNearlyZero());
    Movement->SetMovementMode(MOVE_Swimming);
    Axis(TEXT("DBA.SwimVertical"),1.f);
    TestTrue(TEXT("空格输入上浮"), Character->ConsumeMovementInputVector().Z > .9);
    Axis(TEXT("DBA.SwimVertical"),-1.f);
    TestTrue(TEXT("Ctrl输入下潜"), Character->ConsumeMovementInputVector().Z < -.9);
    Controller->SetControlRotation(FRotator(-45,0,0));
    Axis(TEXT("DBA.MoveForward"),1.f);
    TestTrue(TEXT("水中沿真实视线向下游动"), Character->ConsumeMovementInputVector().Z < -.6);
    Controller->SetIgnoreMoveInput(true);
    Axis(TEXT("DBA.SwimVertical"),1.f);
    TestTrue(TEXT("失控不能继续升降"), Character->ConsumeMovementInputVector().IsNearlyZero());
    Controller->ResetIgnoreMoveInput();
    Movement->SetMovementMode(MOVE_Walking);
    Axis(TEXT("DBA.MoveForward"),1.f);
    TestTrue(TEXT("岸上前进忽略镜头俯仰"), FMath::IsNearlyZero(Character->ConsumeMovementInputVector().Z));
    Axis(TEXT("DBA.SwimVertical"),-1.f);
    TestTrue(TEXT("出水不继承旧下降意图"), Character->ConsumeMovementInputVector().IsNearlyZero());
    World->DestroyWorld(false);
    return true;
}
#endif
