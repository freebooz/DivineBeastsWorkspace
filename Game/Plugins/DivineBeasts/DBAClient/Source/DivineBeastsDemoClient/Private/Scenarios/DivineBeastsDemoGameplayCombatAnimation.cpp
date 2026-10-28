#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Feedback/GamePlatformLocalHitstopSubsystem.h"
#include "Feedback/GamePlatformHitFlashWorldSubsystem.h"
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo {
FGamePlatformResult FGameplayManualState::Combat(FName O,const FManualArguments& A){
 if(O==TEXT("CommitInput"))return Gameplay(O,A);
 if(!Source.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("先创建夹具。"));
 const auto E=Epoch;auto* S=Source.Get();
 if(O==TEXT("LaunchMovingTarget")||O==TEXT("LaunchThenSwitch")||O==TEXT("LaunchThenInvalidate")||O==TEXT("LaunchStraight")||O==TEXT("LaunchBlocked")){
  if(!Target.IsValid()||!Alternate.IsValid()||Projectile.IsValid())return GameplayError(TEXT("Demo.Gameplay.ProjectileCaseReset"),TEXT("每个独立用例先Reset、CreateFixture、ActivateFixture；不能复用上一弹体。"));
  if(!Target->SetActorLocation(FVector(1400,O==TEXT("LaunchStraight")?500:100,100),true)||!Current(E))return GameplayError(TEXT("Demo.Gameplay.CaseTargetMove"),TEXT("真实目标预置失败或运行已失活。"));
  if(!Alternate->SetActorLocation(FVector(700,900,100),true)||!Current(E))return GameplayError(TEXT("Demo.Gameplay.CaseAlternateMove"),TEXT("真实备用目标预置失败或运行已失活。"));
  auto R=Combat(O==TEXT("LaunchStraight")?FName(TEXT("ClearSelection")):FName(TEXT("SelectTarget")),A);if(!R.IsSuccess()||!Current(E))return R;
  if(O==TEXT("LaunchBlocked")){R=Combat(TEXT("PlaceObstacle"),A);if(!R.IsSuccess()||!Current(E))return R;}
  R=Combat(TEXT("Launch"),A);if(!R.IsSuccess()||!Current(E))return R;
  if(O==TEXT("LaunchMovingTarget"))Target->GetCharacterMovement()->Velocity=FVector(0,250,0);
  if(O==TEXT("LaunchThenSwitch"))R=Combat(TEXT("SwitchSelection"),A);
  if(O==TEXT("LaunchThenInvalidate"))R=Combat(TEXT("InvalidateTarget"),A);
  Observe(TEXT("独立用例已在同一权威命令内完成预置/选择/真实Commit/发射及指定后继动作；默认弹体速度未修改，观察真实FlightFacts与世界几何。"));return R;
 }
 if(O==TEXT("CancelProjectile")){if(!Projectile.IsValid())return GameplayError(TEXT("Demo.Gameplay.Projectile"),TEXT("没有本弹体。"));Projectile->AuthorityCancel();return FGamePlatformResult::Success();}
 if(O==TEXT("ToggleObstacle")||O==TEXT("PlaceObstacle")){
  if(O!=TEXT("PlaceObstacle")&&!A.Bool(TEXT("Obstacle"))){if(Obstacle.IsValid()){if(!Obstacle->Destroy())return GameplayError(TEXT("Demo.Gameplay.ObstacleCleanup"),TEXT("障碍未销毁。"));Obstacle.Reset();}return FGamePlatformResult::Success();}
  if(Obstacle.IsValid())return GameplayError(TEXT("Demo.Gameplay.ObstacleExists"),TEXT("障碍已经存在。"));FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
  Obstacle=World->SpawnActor<AActor>(AActor::StaticClass(),FVector(-175,100,120),FRotator::ZeroRotator,P);if(!Current(E)||!Obstacle.IsValid())return GameplayError(TEXT("Demo.Gameplay.ObstacleSpawn"),TEXT("障碍出生失败/来源停止。"));auto* B=NewObject<UBoxComponent>(Obstacle.Get());Obstacle->SetRootComponent(B);Obstacle->AddInstanceComponent(B);B->SetBoxExtent(FVector(12,150,150));B->SetCollisionProfileName(TEXT("BlockAll"));B->RegisterComponent();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("障碍注册期间停止。"));Obstacle->SetActorLocation(FVector(-175,100,120));
  auto* V=NewObject<UStaticMeshComponent>(Obstacle.Get());Obstacle->AddInstanceComponent(V);V->SetupAttachment(B);V->SetCollisionEnabled(ECollisionEnabled::NoCollision);V->SetRelativeScale3D(FVector(.24,3,3));V->SetStaticMesh(Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")).ResolveObject()));V->RegisterComponent();return FGamePlatformResult::Success();
 }
 if(O==TEXT("SwitchSelection"))return Alternate.IsValid()?GameplayBool(S->Combat->AuthoritySelectProjectileTarget(Alternate.Get(),S->GetCombatAvatarGeneration(),Alternate->GetCombatAvatarGeneration()),TEXT("select alternate: already launched projectile keeps frozen original")):GameplayError(TEXT("Demo.Gameplay.Alternate"),TEXT("备用目标失效。"));
 if(O==TEXT("ClearSelection"))return GameplayBool(S->Combat->AuthoritySelectProjectileTarget(nullptr,S->GetCombatAvatarGeneration(),0),TEXT("clear selection; future flight stays straight"));
 if(!Target.IsValid())return GameplayError(TEXT("Demo.Gameplay.Target"),TEXT("目标已失效；Reset后重建。"));
 auto* T=Target.Get();
 if(O==TEXT("SelectTarget"))return GameplayBool(S->Combat->AuthoritySelectProjectileTarget(T,S->GetCombatAvatarGeneration(),T->GetCombatAvatarGeneration()),TEXT("server validates selected target generations"));
 if(O==TEXT("MoveTarget"))return GameplayBool(T->SetActorLocation(FVector(A.Number(TEXT("TargetX")),A.Number(TEXT("TargetY")),100),true),TEXT("real authority swept target movement; original homing target remains"));
 if(O==TEXT("InvalidateTarget")){bool B=T->Destroy();if(B)Target.Reset();return GameplayBool(B,TEXT("destroy original target; never auto-retarget"));}
 FGamePlatformCombatSpec Spec;Spec.Source=S;Spec.Target=T;Spec.SourceAvatarGeneration=S->GetCombatAvatarGeneration();Spec.TargetAvatarGeneration=T->GetCombatAvatarGeneration();Spec.Magnitude=A.Number(TEXT("Magnitude"));Spec.SourceAbilityId=TEXT("Demo.Manual.TrustedCombatFixture");Spec.EventId=FGuid::NewGuid();
 if(O==TEXT("Launch")){
  if(Projectile.IsValid())return GameplayError(TEXT("Demo.Gameplay.ProjectileExists"),TEXT("原弹体仍归本Run；Reset清理再发射。"));
  if(!InputHandle.IsValid()){auto R=GrantInput(GameplayManualInputTag());if(!R.IsSuccess())return R;}
  int32 Before=S->CommittedInputs;bool B=S->ASC->TryActivateAbility(InputHandle);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("Commit通知期间停止。"));if(!B||S->CommittedInputs<=Before)return GameplayError(TEXT("Demo.Gameplay.CommitRejected"),TEXT("真实GAS Commit拒绝，不生成弹体。"));
  int32 Generation=0;Spec.Target=S->Combat->GetSelectedProjectileTarget(2000,Generation);Spec.TargetAvatarGeneration=Generation;FGamePlatformCombatProjectileMotion Motion;
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto Origin=S->Combat->GetCombatTargetPoint()+FVector(65,0,0);Projectile=World->SpawnActor<AGamePlatformCombatProjectile>(AGamePlatformCombatProjectile::StaticClass(),Origin,FRotator::ZeroRotator,P);if(!Current(E)||!Projectile.IsValid())return GameplayError(TEXT("Demo.Gameplay.ProjectileSpawn"),TEXT("弹体出生失败/来源失活；已提交成本不伪造返还。"));
  auto Weak=TWeakPtr<FGameplayManualState>(AsShared());Projectile->OnFlightFactsChanged().AddLambda([Weak,E](auto*,const auto&){if(auto Self=Weak.Pin())if(Self->Current(E))Self->Snapshot();});
  B=Projectile->AuthorityInitialize(Spec,Motion,FVector::ForwardVector);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("弹体启动通知期间停止。"));
  if(B){auto* V=NewObject<UStaticMeshComponent>(Projectile.Get());Projectile->AddInstanceComponent(V);V->SetupAttachment(Projectile->GetRootComponent());V->SetCollisionEnabled(ECollisionEnabled::NoCollision);V->SetRelativeScale3D(FVector(.12));V->SetStaticMesh(Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")).ResolveObject()));V->RegisterComponent();}
  return GameplayBool(B,TEXT("AuthorityInitialize native projectile; no synthetic hit or optional magic VFX"));
 }
 // 平台将非正代次解释为未指定；首代没有正数前代，使用2明确制造失配，仍由真实权威拒绝。
 if(O==TEXT("StaleDamage"))Spec.SourceAvatarGeneration=Spec.SourceAvatarGeneration>1?Spec.SourceAvatarGeneration-1:2;
 FGamePlatformCombatResult R;
 if(O==TEXT("Damage")||O==TEXT("StaleDamage")||O==TEXT("DuplicateDamage")){
  if(O==TEXT("DuplicateDamage")){if(!LastDamageId.IsValid())return GameplayError(TEXT("Demo.Gameplay.NoPriorDamage"),TEXT("先执行一次Damage。"));Spec.EventId=LastDamageId;}else LastDamageId=Spec.EventId;
  R=S->Combat->ApplyDamage(Spec);
 }else if(O==TEXT("Heal"))R=S->Combat->ApplyHealing(Spec);
 else if(O==TEXT("Shield"))R=S->Combat->ApplyShield(Spec,A.Number(TEXT("Duration")));
 else if(O==TEXT("Stun")||O==TEXT("Silence"))R=S->Combat->ApplyControl(Spec,O==TEXT("Stun")?EGamePlatformControlType::Stun:EGamePlatformControlType::Silence,A.Number(TEXT("Duration")));
 else if(O==TEXT("Slow"))R=S->Combat->ApplySlow(Spec,A.Number(TEXT("SlowFraction")),A.Number(TEXT("Duration")));
 else if(O==TEXT("RemoveControl")){bool B=T->Combat->RemoveControl(EGamePlatformControlType::Stun);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("控制事件期间停止。"));B=T->Combat->RemoveControl(EGamePlatformControlType::Silence)||B;if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("控制事件期间停止。"));B=T->Combat->RemoveControl(EGamePlatformControlType::Slow)||B;return GameplayBool(B,TEXT("RemoveControl on owned target"));}
 else if(O==TEXT("Push")||O==TEXT("Pull"))R=S->Combat->ApplyForcedDisplacement(Spec,O==TEXT("Push")?EGamePlatformDisplacementMode::PushAway:EGamePlatformDisplacementMode::PullToward,S->GetActorLocation(),A.Number(TEXT("Speed")));
 else return GameplayError(TEXT("Demo.Gameplay.CombatOperation"),TEXT("未知战斗操作。"));
 Observe(FString::Printf(TEXT("真实权威结果=%d Event=%s Requested=%.2f Final=%.2f AppliedHealth=%.2f AppliedShield=%.2f RemainingHealth=%.2f RemainingShield=%.2f；不声明音画播放。"),int32(R.Error),*R.EventId.ToString(),R.RequestedMagnitude,R.FinalMagnitude,R.AppliedToHealth,R.AppliedToShield,R.RemainingHealth,R.RemainingShield));return GameplayBool(R.IsSuccess(),TEXT("GamePlatformCombat trusted local fixture API"));
}
FGamePlatformResult FGameplayManualState::Animation(FName O,const FManualArguments& A){
 if(!Source.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("先创建夹具。"));auto* F=Source.Get();auto* M=F->GetMesh();
 if(O==TEXT("MoveCharacter")){F->GetCharacterMovement()->Velocity=FVector(A.Number(TEXT("Speed")),0,0);return FGamePlatformResult::Success();}
 if(O==TEXT("JumpCharacter")){F->LaunchCharacter(FVector(0,0,F->GetCharacterMovement()->JumpZVelocity),false,true);return FGamePlatformResult::Success();}
 if(O==TEXT("StopMovement")){F->GetCharacterMovement()->StopMovementImmediately();return FGamePlatformResult::Success();}
 if(O==TEXT("Hitstop")||O==TEXT("DuplicateHitstop")){auto* H=Player->GetSubsystem<UGamePlatformLocalHitstopSubsystem>();if(!H||!M->GetSkeletalMeshAsset())return GameplayError(TEXT("Demo.Gameplay.HitstopHost"),TEXT("需真实LP顿帧服务与已加载骨骼动画。"));if(O==TEXT("Hitstop"))HitstopId=FGuid::NewGuid();return GameplayBool(H->ApplyVisualHitstop(HitstopId,M,nullptr,A.Int(TEXT("Frames"))),TEXT("ApplyVisualHitstop (optional decoration only)"));}
 if(O==TEXT("HitFlash")||O==TEXT("DuplicateHitFlash")){auto* H=World->GetSubsystem<UGamePlatformHitFlashWorldSubsystem>();if(!H||!Overlay.IsValid())return GameplayError(TEXT("Demo.Gameplay.HitflashHost"),TEXT("需真实World闪白服务与本租约Overlay。"));if(O==TEXT("HitFlash"))FlashId=FGuid::NewGuid();return GameplayBool(H->PlayHitFlash(FlashId,F->Marker.Get(),Overlay.Get(),A.Number(TEXT("Duration"))),TEXT("PlayHitFlash: marker geometry, no server authority"));}
 return GameplayError(TEXT("Demo.Gameplay.AnimationOperation"),TEXT("未知动画操作。"));
}
}
