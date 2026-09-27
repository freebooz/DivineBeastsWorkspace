#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/GamePlatformCombatStateProvider.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformCombatResult.h"
#include "Types/GamePlatformCombatSpec.h"
#include "GamePlatformCombatComponent.generated.h"

class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatAttributeSet;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformCombatEventDelegate,
    const FGamePlatformCombatEvent&, Event);

/**
 * 服务器权威战斗状态组件。
 * 客户端只观察复制结果，不提供任意客户端通用伤害RPC。
 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMCOMBAT_API UGamePlatformCombatComponent final
    : public UActorComponent
    , public IGamePlatformCombatStateProvider
{
    GENERATED_BODY()

public:
    UGamePlatformCombatComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Combat")
    UGamePlatformAbilitySystemComponent* GetAbilitySystemComponent() const
    {
        return AbilitySystemComponent;
    }

    UFUNCTION(BlueprintPure, Category="Combat")
    UGamePlatformCombatAttributeSet* GetCombatAttributeSet() const;

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetCombatHealth() const override;

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetCombatMaxHealth() const override;

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetCombatShield() const override;

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetCombatMaxShield() const override;

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual bool IsCombatDead() const override { return bDead; }

    UFUNCTION(BlueprintPure, Category="Combat")
    virtual int32 GetCombatAvatarGeneration() const override
    {
        return AvatarGeneration;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    FGamePlatformCombatResult ApplyDamage(
        const FGamePlatformCombatSpec& InSpec);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    FGamePlatformCombatResult ApplyHealing(
        const FGamePlatformCombatSpec& InSpec);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    FGamePlatformCombatResult ApplyControl(
        const FGamePlatformCombatSpec& InSpec,
        EGamePlatformControlType ControlType,
        float DurationSeconds);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    bool RemoveControl(EGamePlatformControlType ControlType);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    bool ResetForNewAvatar(
        int32 NewAvatarGeneration,
        float HealthFraction = 1.0f,
        float ShieldFraction = 0.0f);

    /** AttributeSet专用结算回调，不作为客户端请求API。 */
    void ResolveIncomingDamage(
        UGamePlatformCombatAttributeSet& Attributes,
        const FGameplayEffectSpec& EffectSpec,
        float FinalDamage);

    /** AttributeSet专用结算回调，不作为客户端请求API。 */
    void ResolveIncomingHealing(
        UGamePlatformCombatAttributeSet& Attributes,
        const FGameplayEffectSpec& EffectSpec,
        float FinalHealing);

    UPROPERTY(BlueprintAssignable, Category="Combat")
    FGamePlatformCombatEventDelegate OnCombatEvent;

private:
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystemComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformCombatAttributeSet> CombatAttributeSet = nullptr;

    UPROPERTY(ReplicatedUsing=OnRep_Dead)
    bool bDead = false;

    UPROPERTY(Replicated)
    int32 AvatarGeneration = 1;

    UPROPERTY(Replicated)
    int32 WorldContextGeneration = 1;

    TArray<FGamePlatformCombatSpec> PendingResolutionStack;
    TMap<FGuid, FGamePlatformCombatResult> ResolvedResults;
    TSet<FGuid> CompletedEventIds;
    TArray<FGuid> CompletedEventOrder;

    static constexpr int32 MaxRememberedEventIds = 256;

    UFUNCTION()
    void OnRep_Dead();

    void HandleControlTagChanged(const FGameplayTag Tag, int32 NewCount);

    EGamePlatformCombatError ValidateSpec(
        const FGamePlatformCombatSpec& Spec,
        bool bHealing,
        bool bCheckSelfPolicy = true) const;

    FGamePlatformCombatSpec NormalizeSpec(
        const FGamePlatformCombatSpec& InSpec) const;

    FGamePlatformCombatResult ApplyInstantEffect(
        const FGamePlatformCombatSpec& InSpec,
        TSubclassOf<UGameplayEffect> EffectClass,
        const FGameplayTag& MagnitudeTag,
        bool bHealing);

    void MarkEventCompleted(const FGuid& EventId);
    bool HasCompletedEvent(const FGuid& EventId) const;

    void EnterDeadState(
        const FGamePlatformCombatSpec& SourceSpec,
        FGamePlatformCombatResult& InOutResult);

    void PublishCombatEvent(
        EGamePlatformCombatEventType EventType,
        const FGamePlatformCombatSpec& SourceSpec,
        const FGamePlatformCombatResult& Result);

    void ExecuteGameplayCue(
        const FGameplayTag& CueTag,
        const FGamePlatformCombatSpec& SourceSpec,
        const FGamePlatformCombatResult& Result);

    FGamePlatformCombatSpec GetPendingResolutionSpec(
        const FGameplayEffectSpec& EffectSpec) const;
};
