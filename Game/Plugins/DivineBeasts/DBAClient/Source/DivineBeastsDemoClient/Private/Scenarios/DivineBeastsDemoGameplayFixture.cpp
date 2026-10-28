#include "Scenarios/DivineBeastsDemoGameplayFixture.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
ADivineBeastsDemoGameplayFixture::ADivineBeastsDemoGameplayFixture(){
 PrimaryActorTick.bCanEverTick=false;AutoPossessAI=EAutoPossessAI::Disabled;GetCharacterMovement()->bRunPhysicsWithNoController=true;
 ASC=CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("ASC"));
 Combat=CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("Combat"));
 Eligibility=CreateDefaultSubobject<UGamePlatformGameplayEligibilityComponent>(TEXT("Eligibility"));
 CastFacts=CreateDefaultSubobject<UGamePlatformAbilityCastComponent>(TEXT("CastFacts"));
 AITarget=CreateDefaultSubobject<UGamePlatformAITargetComponent>(TEXT("AITarget"));
 AIState=CreateDefaultSubobject<UGamePlatformAIStateComponent>(TEXT("AIState"));
 Marker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureMarker"));Marker->SetupAttachment(GetRootComponent());Marker->SetRelativeScale3D(FVector(.5,.5,1));Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
UAbilitySystemComponent* ADivineBeastsDemoGameplayFixture::GetAbilitySystemComponent()const{return ASC;}
bool ADivineBeastsDemoGameplayFixture::CanSourceCombat()const{return HasAuthority()&&Eligibility->IsServerPlayerActiveForGameplay()&&!Combat->IsCombatDead();}
bool ADivineBeastsDemoGameplayFixture::CanReceiveCombat()const{return HasAuthority()&&Eligibility->IsServerPlayerActiveForGameplay()&&!Combat->IsCombatDead();}
int32 ADivineBeastsDemoGameplayFixture::GetCombatAvatarGeneration()const{return Combat->GetCombatAvatarGeneration();}
UDivineBeastsDemoGameplayCost::UDivineBeastsDemoGameplayCost(){DurationPolicy=EGameplayEffectDurationType::Instant;FGameplayModifierInfo M;M.Attribute=UGamePlatformCombatAttributeSet::GetEnergyAttribute();M.ModifierOp=EGameplayModOp::Additive;M.ModifierMagnitude=FScalableFloat(-25.f);Modifiers.Add(M);}
UDivineBeastsDemoGameplayInput::UDivineBeastsDemoGameplayInput(){NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;CostGameplayEffectClass=UDivineBeastsDemoGameplayCost::StaticClass();}
void UDivineBeastsDemoGameplayInput::ActivateAbility(const FGameplayAbilitySpecHandle H,const FGameplayAbilityActorInfo* A,const FGameplayAbilityActivationInfo I,const FGameplayEventData*){
 bool B=CommitAbility(H,A,I);if(B)if(auto* F=Cast<ADivineBeastsDemoGameplayFixture>(A->AvatarActor.Get()))++F->CommittedInputs;EndAbility(H,A,I,true,!B);
}
