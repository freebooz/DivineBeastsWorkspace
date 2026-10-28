// Development-owned fixture composition; no production admission or account identity.
#pragma once
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/GamePlatformCombatant.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "GameplayEffect.h"
#include "Types/GamePlatformAITypes.h"
#include "DivineBeastsDemoGameplayFixture.generated.h"
class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatComponent;
class UGamePlatformGameplayEligibilityComponent;
class UGamePlatformAbilityCastComponent;
class UGamePlatformAITargetComponent;
class UGamePlatformAIStateComponent;
class UStaticMeshComponent;
UCLASS(Transient,NotBlueprintable)
class ADivineBeastsDemoGameplayFixture final:public ACharacter,public IAbilitySystemInterface,public IGamePlatformCombatant {
 GENERATED_BODY()
public:
 ADivineBeastsDemoGameplayFixture();
 UAbilitySystemComponent* GetAbilitySystemComponent()const override;
 UGamePlatformAbilitySystemComponent* GetGamePlatformAbilitySystemComponent()const override{return ASC;}
 UGamePlatformCombatComponent* GetGamePlatformCombatComponent()const override{return Combat;}
 bool CanSourceCombat()const override;
 bool CanReceiveCombat()const override;
 int32 GetCombatAvatarGeneration()const override;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformAbilitySystemComponent> ASC;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformCombatComponent> Combat;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformGameplayEligibilityComponent> Eligibility;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformAbilityCastComponent> CastFacts;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformAITargetComponent> AITarget;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UGamePlatformAIStateComponent> AIState;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UStaticMeshComponent> Marker;
 int32 CommittedInputs=0;
};
/** One bounded test cost: only this native fixture ability. Never alters existing spell defaults. */
UCLASS(Transient)
class UDivineBeastsDemoGameplayCost final:public UGameplayEffect {
 GENERATED_BODY()
public:UDivineBeastsDemoGameplayCost();
};
UCLASS(Transient,NotBlueprintable)
class UDivineBeastsDemoGameplayInput final:public UGamePlatformGameplayAbility {
 GENERATED_BODY()
public:
 UDivineBeastsDemoGameplayInput();
 void ActivateAbility(const FGameplayAbilitySpecHandle,const FGameplayAbilityActorInfo*,const FGameplayAbilityActivationInfo,const FGameplayEventData*)override;
};
UCLASS(Transient)
class UDivineBeastsDemoGameplayObserver final:public UObject {
 GENERATED_BODY()
public:
 TFunction<void()> Changed;
 UFUNCTION()void OnAIChanged(const FGamePlatformAIStateSnapshot& Snapshot){if(Changed)Changed();}
};
