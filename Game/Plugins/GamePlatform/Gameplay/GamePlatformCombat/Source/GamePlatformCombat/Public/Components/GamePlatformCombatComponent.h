#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/GamePlatformCombatStateProvider.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformCombatFeedbackNetEvent.h"
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

    /**
     * 服务器应用单次有期护盾GameplayEffect（玩法效果）。
     * InSpec.Magnitude为独立的吸收容量，DurationSeconds为GE存活秒数；
     * 不创建或修改任何Shield/MaxShield GAS属性，返回值仅含本次效果结果。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Effects")
    FGamePlatformCombatResult ApplyShield(
        const FGamePlatformCombatSpec& InSpec,
        float DurationSeconds);

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
        float HealthFraction = 1.0f);

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

    /** 单一护盾效果实例账本：权威容量与真实GAS效果句柄共同决定可吸收量，不复制数值属性。 */
    struct FActiveShieldEffectCharge
    {
        FActiveGameplayEffectHandle EffectHandle;
        float RemainingCapacity = 0.0f;
    };

    /** 有界服务器瞬时状态；失效GE惰性清理，角色死亡/重生/离开时立即清理。 */
    TArray<FActiveShieldEffectCharge> ActiveShieldEffects;
    static constexpr int32 MaxActiveShieldEffects = 16;

    float GetAvailableShieldEffectCapacity() const;
    void CompactExpiredShieldEffects();
    void ConsumeShieldEffectCapacity(float RequestedAbsorption);
    void ClearShieldEffects();

    /**
     * 服务器单向向相关Actor客户端广播最小的已确认表现事实。
     * Unreliable仅影响可选表现，绝不承担生命、伤害、移动或技能权威结算。
     */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastConfirmedCombatFeedback(
        const FGamePlatformCombatFeedbackNetEvent& Feedback);

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
