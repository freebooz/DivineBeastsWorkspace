#if WITH_DEV_AUTOMATION_TESTS
// 真实ASC的注入、未Active与换Avatar拒绝；夹具仅提供可控资格事实，不授予生产能力。
#include "Misc/AutomationTest.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
namespace
{
class FActivationGateFixture final : public IGamePlatformAbilityActivationGate
{
public:
    bool bActive = false;
    FGamePlatformResult Evaluate(const UGamePlatformAbilitySystemComponent&) const override
    { return bActive ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(TEXT("GameplayNotActive"), TEXT("测试资格未Active。")); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityActivationGateLifecycleTest,
    "GamePlatform.AbilitySystem.ActivationGate.AvatarLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAbilityActivationGateLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
    APawn* Owner = World->SpawnActor<APawn>(); APawn* NextAvatar = World->SpawnActor<APawn>();
    auto* Component = NewObject<UGamePlatformAbilitySystemComponent>(Owner); Owner->AddInstanceComponent(Component); Component->RegisterComponent();
    Component->InitAbilityActorInfo(Owner, Owner);
    TestTrue(TEXT("旧原生绑定首次平台注册仍签发代次"), Component->BindAbilityActorInfo(Owner, Owner));
    TestTrue(TEXT("首个平台绑定具有正代次"), Component->GetAvatarBindingSnapshot().AvatarGeneration > 0);
    TestFalse(TEXT("未注入默认拒绝"), Component->EvaluateActivationEligibility().IsSuccess());
    auto Gate = MakeShared<FActivationGateFixture>();
    TestTrue(TEXT("就绪后同世界注入"), Component->SetActivationGate(Owner, Gate).IsSuccess());
    TestFalse(TEXT("未Gameplay Active拒绝"), Component->EvaluateActivationEligibility().IsSuccess());
    Gate->bActive = true; TestTrue(TEXT("当前Avatar真实资格允许评估"), Component->EvaluateActivationEligibility().IsSuccess());
    Component->BindAbilityActorInfo(Owner, NextAvatar);
    TestFalse(TEXT("旧Avatar注入不能授权新Avatar"), Component->EvaluateActivationEligibility().IsSuccess());
    TestTrue(TEXT("新绑定须重新注入"), Component->SetActivationGate(Owner, Gate).IsSuccess());
    Component->ClearAbilityAvatar(); TestFalse(TEXT("失去Avatar拒绝"), Component->EvaluateActivationEligibility().IsSuccess());
    World->DestroyWorld(false); return true;
}
#endif
