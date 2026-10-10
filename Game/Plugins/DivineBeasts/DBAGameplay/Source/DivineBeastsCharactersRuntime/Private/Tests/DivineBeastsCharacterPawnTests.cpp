// 项目角色实体回归：验证真实默认组件、拥有事件的ASC绑定，以及默认失败关闭；不依赖地图/英雄资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Types/GamePlatformCombatEvent.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterPawnTest,
    "DivineBeasts.Character.Pawn.AuthoritativeComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterPawnTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Pawn = World->SpawnActor<ADivineBeastsCharacter>();
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* ASC = Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    auto* Combat = Pawn->FindComponentByClass<UGamePlatformCombatComponent>();
    auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
    TestNotNull(TEXT("真实Pawn默认拥有ASC"), ASC);
    TestNotNull(TEXT("真实Pawn默认拥有战斗组件"), Combat);
    TestNotNull(TEXT("真实Pawn默认拥有权威资格组件"), Eligibility);
    TestFalse(TEXT("初始化和准入前不得Active"), Eligibility->IsServerPlayerActiveForGameplay());
    FString Error;
    auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    TestFalse(TEXT("未签发/冷加载租约不能交接为Ready"), Identity->TryUsePreloadedDefinition({}, *Pawn, Error));
    Controller->Possess(Pawn);
    TestTrue(TEXT("实际Possess绑定ActorInfo"), ASC->GetAvatarActor() == Pawn && ASC->GetOwnerActor() == Pawn);
    TestFalse(TEXT("缺Definition时实际激活门禁拒绝"), ASC->EvaluateActivationEligibility().IsSuccess());
    Controller->UnPossess();
    TestFalse(TEXT("失去拥有关系撤销ASC Avatar"), ASC->GetAvatarBindingSnapshot().bBound);
    TestFalse(TEXT("失去拥有关系撤销资格"), Eligibility->IsServerPlayerActiveForGameplay());
    World->DestroyWorld(false);
    return true;
}
// 真实Combat属性结算使生命归零，Pawn原生事实每代次只发布一次；测试不绕过为项目Definition Ready。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterDeathFactTest,
    "DivineBeasts.Character.Pawn.AuthoritativeDeathOncePerAvatar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterDeathFactTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    if (!TestNotNull(TEXT("死亡事实测试世界"), World)) { return false; }
    // CreateWorld尚未路由Actor初始化；ProcessEvent在AreActorsInitialized=false时拒绝动态UFUNCTION死亡回调。
    // 走引擎真实初始化，不设置脚本放行开关、不直接调用项目死亡处理函数。
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters PawnSpawnParameters; PawnSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn = World->SpawnActor<ADivineBeastsCharacter>(PawnSpawnParameters);
    if (!TestNotNull(TEXT("死亡事实真实Pawn"), Pawn)) { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return false; }
    Pawn->DispatchBeginPlay();
    if (!TestTrue(TEXT("死亡事实使用真实已初始化Actor"), World->AreActorsInitialized() && Pawn->IsActorInitialized() && Pawn->HasActorBegunPlay()))
    { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return false; }
    auto* Combat = Pawn->GetGamePlatformCombatComponent(); auto* Attributes = Combat->GetCombatAttributeSet();
    if (!TestNotNull(TEXT("真实Combat持有核心属性"), Attributes) ||
        !TestTrue(TEXT("项目Pawn已订阅真实Combat动态事件"), Combat->OnCombatEvent.IsBound()))
    { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return false; }
    int32 Published = 0; FGamePlatformCombatEvent Death;
    Pawn->OnAuthoritativeDeath().AddLambda([&](const auto& Event) { ++Published; Death = Event; });
    if (Attributes)
    {
        FGameplayEffectSpec Effect; Combat->ResolveIncomingDamage(*Attributes, Effect, Attributes->GetHealth() + 1.0f);
        TestTrue(TEXT("真实结算已进入死亡"), Combat->IsCombatDead()); TestEqual(TEXT("目标死亡事实一次"), Published, 1);
        Combat->OnCombatEvent.Broadcast(Death); TestEqual(TEXT("相同事实重复不再发布"), Published, 1);
        auto Old = Death; --Old.TargetAvatarGeneration; Old.EventId = FGuid::NewGuid(); Combat->OnCombatEvent.Broadcast(Old);
        TestEqual(TEXT("旧代次不再发布"), Published, 1);
    }
    Pawn->OnAuthoritativeDeath().Clear(); World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return true;
}
#endif
