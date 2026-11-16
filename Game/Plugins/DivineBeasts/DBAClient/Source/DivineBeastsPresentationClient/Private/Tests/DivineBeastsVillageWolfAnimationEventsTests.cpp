// SSLM狼动画事件链回归：真实狼/平台Cast/Combat和已保存Mesh、AnimBP、Montage。
// 独立世界不连后端、不保存资产；验证首快照、攻击去重/取消、真实死亡与解绑，不代替网络和画面验收。
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Animation/AnimMontage.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creatures/DivineBeastsVillageWolfAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageWolfAnimationEventsTest,
    "DivineBeasts.Presentation.VillageWolf.AnimationEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsVillageWolfAnimationEventsTest::RunTest(const FString&)
{
    const TCHAR* Base = TEXT("/DBAWorldPack_Village/Creatures/Wolf/");
    auto* Asset = LoadObject<USkeletalMesh>(nullptr, *(FString(Base) + TEXT("SK_DBA_VillageWolf.SK_DBA_VillageWolf")));
    auto* Montage = LoadObject<UAnimMontage>(nullptr, *(FString(Base) + TEXT("AM_DBA_VillageWolf_Bite.AM_DBA_VillageWolf_Bite")));
    UClass* AnimationClass = LoadClass<UAnimInstance>(nullptr, *(FString(Base) + TEXT("ABP_DBA_VillageWolf.ABP_DBA_VillageWolf_C")));
    if (!TestNotNull(TEXT("已保存真实狼Mesh"), Asset) || !TestNotNull(TEXT("已保存咬击Montage"), Montage) ||
        !TestNotNull(TEXT("已保存狼AnimBP"), AnimationClass)) return false;
    if (!TestTrue(TEXT("狼AnimBP派生实际事件适配类"), AnimationClass->IsChildOf(UDivineBeastsVillageWolfAnimInstance::StaticClass())) ||
        !TestTrue(TEXT("狼Montage与Mesh骨架相同"), Asset->GetSkeleton() == Montage->GetSkeleton())) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("独立真实Game世界"), World)) return false;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Wolf = World->SpawnActor<ADivineBeastsVillageWolf>(Spawn);
    auto* Controller = World->SpawnActor<APlayerController>(Spawn);
    if (!TestNotNull(TEXT("真实狼实体"), Wolf) || !TestNotNull(TEXT("隔离占有者"), Controller)) return false;
    Wolf->DispatchBeginPlay();
    Controller->Possess(Wolf);
    if (!TestTrue(TEXT("权威Combat初始化75生命"), Wolf->AuthorityInitializeWolf(75.0f))) return false;
    auto* Mesh = Wolf->GetMesh();
    Mesh->SetSkeletalMeshAsset(Asset);
    Mesh->SetAnimInstanceClass(AnimationClass);
    auto* Animation = Cast<UDivineBeastsVillageWolfAnimInstance>(Mesh->GetAnimInstance());
    auto* CastComponent = Wolf->FindComponentByClass<UGamePlatformAbilityCastComponent>();
    if (!TestNotNull(TEXT("引擎实际创建原生派生实例"), Animation) ||
        !TestNotNull(TEXT("基类原有唯一Cast组件"), CastComponent)) return false;
    TestEqual(TEXT("内容包默认值绑定正式咬击Montage"), Animation->BiteMontage.Get(), Montage);
    TestFalse(TEXT("首次快照为存活且无伪造死亡"), Animation->bWolfDead);
    TestTrue(TEXT("生命订阅按动画实例绑定"), Wolf->OnWolfLifeChanged().IsBoundToObject(Animation));
    TestTrue(TEXT("攻击订阅按动画实例绑定"), CastComponent->OnCastFactsChanged().IsBoundToObject(Animation));

    Wolf->GetCharacterMovement()->Velocity = FVector(240, 0, 0);
    Animation->NativeUpdateAnimation(1.0f / 30.0f);
    TestEqual(TEXT("真实CharacterMovement速度进入BlendSpace输入"), Animation->WolfSpeed, 240.0f);
    const FGuid First = FGuid::NewGuid();
    TestTrue(TEXT("真实平台API发布服务器确认攻击"), CastComponent->PublishConfirmedCast(First,
        TEXT("VillageWolf.Bite"), TEXT("VillageWolf.Bite"), Wolf->GetCombatAvatarGeneration(), 0.8f));
    TestTrue(TEXT("复制事实通知使真实Montage开始"), Animation->Montage_IsPlaying(Montage));
    Animation->Montage_SetPosition(Montage, 0.25f);
    CastComponent->OnCastFactsChanged().Broadcast(CastComponent, CastComponent->GetCastFacts());
    TestTrue(TEXT("重复快照不把咬击重置回起点"), FMath::IsNearlyEqual(Animation->Montage_GetPosition(Montage), 0.25f));
    TestTrue(TEXT("真实平台取消受理"), CastComponent->CancelConfirmedCast(First));
    TestFalse(TEXT("取消通知立即停止自有攻击"), Animation->Montage_IsPlaying(Montage));

    TestTrue(TEXT("第二次攻击独立播放"), CastComponent->PublishConfirmedCast(FGuid::NewGuid(),
        TEXT("VillageWolf.Bite"), TEXT("VillageWolf.Bite"), Wolf->GetCombatAvatarGeneration(), 0.8f));
    auto* Combat = Wolf->GetGamePlatformCombatComponent();
    auto* Attributes = Combat ? Combat->GetCombatAttributeSet() : nullptr;
    if (!TestNotNull(TEXT("真实权威Combat属性"), Attributes)) return false;
    FGameplayEffectSpec Effect;
    Combat->ResolveIncomingDamage(*Attributes, Effect, 100.0f);
    TestTrue(TEXT("真实Combat伤害已结算死亡"), Combat->IsCombatDead() && Wolf->IsWolfDead());
    TestEqual(TEXT("死亡后禁用移动防止失去胶囊碰撞时跌入地面"),
        Wolf->GetCharacterMovement()->MovementMode, MOVE_None);
    TestTrue(TEXT("死亡通知在没有动画Tick时立即收敛"), Animation->bWolfDead);
    TestEqual(TEXT("死亡立即归零移动动画输入"), Animation->WolfSpeed, 0.0f);
    TestFalse(TEXT("死亡不再播放咬击"), Animation->Montage_IsPlaying(Montage));
    TestFalse(TEXT("死亡链撤销服务器Cast窗口"), CastComponent->GetCastFacts().bActive);

    Animation->NativeUninitializeAnimation();
    TestFalse(TEXT("卸载解绑生命通知"), Wolf->OnWolfLifeChanged().IsBoundToObject(Animation));
    TestFalse(TEXT("卸载解绑攻击通知"), CastComponent->OnCastFactsChanged().IsBoundToObject(Animation));
    // 真正更换Mesh动画类再恢复，验证晚到/重载观察者以首快照恢复尸体而非重播咬击。
    Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());
    Mesh->SetAnimInstanceClass(AnimationClass);
    auto* Reopened = Cast<UDivineBeastsVillageWolfAnimInstance>(Mesh->GetAnimInstance());
    if (!TestNotNull(TEXT("引擎重建狼动画实例"), Reopened)) return false;
    TestTrue(TEXT("死亡后重载不等下一帧即读取死亡首快照"), Reopened->bWolfDead);
    TestFalse(TEXT("死亡后重载不恢复已取消攻击"), Reopened->Montage_IsPlaying(Montage));
    Reopened->NativeUninitializeAnimation();
    return !HasAnyErrors();
}
#endif // WITH_DEV_AUTOMATION_TESTS
