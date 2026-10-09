#if WITH_DEV_AUTOMATION_TESTS
// 项目组合根在ASC已绑定/随后换Avatar/结束时真实接入和撤销；无就绪和Active事实不能被假成功放行。
#include "Misc/AutomationTest.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterActivationLifecycleTest,
    "DivineBeasts.Characters.ActivationGate.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterActivationLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Pawn = World->SpawnActor<ACharacter>(); auto* OtherPawn = World->SpawnActor<ACharacter>();
    auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Pawn); Pawn->AddInstanceComponent(ASC); ASC->RegisterComponent();
    ASC->BindAbilityActorInfo(Pawn, Pawn);
    auto* Character = NewObject<UDivineBeastsCharacterComponent>(Pawn); Pawn->AddInstanceComponent(Character);
    Character->RegisterComponent(); Character->BeginPlay();
    TestEqual(TEXT("真实项目Gate已注入但角色未Ready必须拒绝"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("CharacterNotReady")));
    ASC->BindAbilityActorInfo(Pawn, OtherPawn);
    TestFalse(TEXT("旧角色组件不能授权另一个Avatar"), ASC->EvaluateActivationEligibility().IsSuccess());
    ASC->BindAbilityActorInfo(Pawn, Pawn);
    TestEqual(TEXT("重新绑定事件重新注入只读Gate"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("CharacterNotReady")));
    Character->EndPlay(EEndPlayReason::Destroyed);
    TestEqual(TEXT("组件结束撤销注入"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("ActivationGateUnavailable")));
    World->DestroyWorld(false); return true;
}
#endif
