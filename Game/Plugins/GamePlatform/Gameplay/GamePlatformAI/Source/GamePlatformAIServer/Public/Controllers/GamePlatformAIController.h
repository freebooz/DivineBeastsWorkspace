#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "Types/GamePlatformAITypes.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformDataLease.h"
#include "GamePlatformAIController.generated.h"


class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UBehaviorTree;
class UBlackboardData;
class UGamePlatformAIDefinition;
class UGamePlatformAIStateComponent;
class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatComponent;
struct FGamePlatformAbilityAvatarBindingSnapshot;

USTRUCT()
struct FGamePlatformAITargetCandidate
{
    GENERATED_BODY()

    TWeakObjectPtr<AActor> Actor;
    FGuid EntityId;
    int32 Generation = 0;
    FVector LastKnownLocation = FVector::ZeroVector;
    double LastSensedTime = 0.0;
    bool bVisible = false;
    bool bHeard = false;
    bool bDamageSource = false;
};

UCLASS()
class GAMEPLATFORMAISERVER_API AGamePlatformAIController
    : public AAIController
{
    GENERATED_BODY()

public:
    AGamePlatformAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category="AI")
    EGamePlatformAIError GetLastAIError() const { return LastError; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    bool TryAttackCurrentTarget();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    void ForceDecisionUpdate();

    /** 游戏线程只读服务器AI资格；注入Gate保存的资源/AI代次必须仍匹配，占有/ActorInfo/Brain/Data均真实就绪才成功。 */
    FGamePlatformResult EvaluateAIAbilityEligibility(const UGamePlatformAbilitySystemComponent& Component,
        int32 ExpectedResourceGeneration, int32 ExpectedAIInstanceGeneration) const;

protected:
    UPROPERTY(EditDefaultsOnly, Category="AI")
    TSoftObjectPtr<UGamePlatformAIDefinition> DefaultDefinition;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAIPerceptionComponent> PlatformPerceptionComponent;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Damage> DamageConfig;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformAIStateComponent> StateComponent;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformAIDefinition> ActiveDefinition;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformCombatComponent> CombatComponent;

    /** 世界/控制器作用域Data租约；按脑资源→定义顺序归还，不创建本域资产管理器。 */
    FGamePlatformDataLease DefinitionResourceLease;
    FGamePlatformDataLease BrainResourceLease;
    FSoftObjectPath DefinitionResourcePath;
    int32 ResourceRequestGeneration = 0;
    int32 ResourceAIInstanceGeneration = 0;
    /** 在停止脑执行之后撤销本控制器全部资源需求，失效迟到完成回调。 */
    void ReleaseResourceLeases();
    /** ActorInfo建立/Brain完成/重生后重新注入中立AI Gate；不套用项目玩家PlayerState规则。 */
    void RefreshAIActivationGate();
    void HandleAbilityAvatarBindingChanged(const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot);
    /** 失去Pawn/退出解绑事件；仅撤销本Controller Gate与本Controller亲自建立的Pawn ActorInfo。 */
    void ClearAIActivationGate(bool bDetachAvatarBinding);
    FDelegateHandle AvatarBindingChangedHandle;
    bool bOwnsAbilityActorInfo = false;

    TMap<TWeakObjectPtr<AActor>, FGamePlatformAITargetCandidate> Candidates;
    TWeakObjectPtr<AActor> CurrentTarget;

    FVector HomeLocation = FVector::ZeroVector;
    FVector LastMoveGoal = FVector::ZeroVector;

    FTimerHandle DecisionTimer;
    double NextAttackTime = 0.0;
    double NextPatrolSelectionTime = 0.0;
    double LastMoveRequestTime = -1.0;

    FDelegateHandle StunTagHandle;
    FDelegateHandle SilenceTagHandle;

    EGamePlatformAIError LastError = EGamePlatformAIError::None;
    bool bBrainReady = false;
    bool bStoppedForDeath = false;

    UFUNCTION()
    void HandleTargetPerceptionUpdated(
        AActor* Actor,
        FAIStimulus Stimulus);

    UFUNCTION()
    void HandleCandidateDestroyed(AActor* DestroyedActor);

    UFUNCTION()
    void HandleCombatEvent(const FGamePlatformCombatEvent& Event);

    void HandleStunTagChanged(const FGameplayTag Tag, int32 NewCount);
    void HandleSilenceTagChanged(const FGameplayTag Tag, int32 NewCount);

    void BeginDefinitionLoad();
    void HandleDefinitionLoaded(int32 ExpectedGeneration);
    void BeginBrainAssetLoad(int32 ExpectedGeneration);
    void HandleBrainAssetsLoaded(int32 ExpectedGeneration);

    bool InitializeDefinition(UGamePlatformAIDefinition& Definition);
    void ConfigurePerception(const UGamePlatformAIDefinition& Definition);
    bool StartBehaviorTreeBrain(const UGamePlatformAIDefinition& Definition);
    void StopBrain(const FString& Reason);

    void StartDecisionTimer();
    void StopDecisionTimer();
    void EvaluateDecision();

    void PruneCandidates();
    void UpsertCandidate(
        AActor* Actor,
        const FVector& Location,
        bool bVisible,
        bool bHeard,
        bool bDamageSource);

    AActor* SelectBestTarget();
    bool IsCandidateEligible(
        const FGamePlatformAITargetCandidate& Candidate) const;

    void SetCurrentTarget(AActor* NewTarget);
    void UpdateBlackboardForTarget(AActor* Target);
    void UpdateNoTargetBehavior();
    void UpdateTargetBehavior(AActor& Target);

    bool RequestPatrolGoal();
    void RequestReturnHome();
    void RequestChase(AActor& Target);

    void SetPublicState(
        EGamePlatformAIPublicState NewState,
        const FGameplayTag& MovementIntent = FGameplayTag(),
        const FGameplayTag& CombatIntent = FGameplayTag());

    bool IsSelfDead() const;
    bool IsSelfStunned() const;
    bool IsSelfSilenced() const;

    int32 GetCurrentGeneration() const;
    double GetServerTimeSeconds() const;

    void BindCombatSignals();
    void UnbindCombatSignals();
    void ResetRuntimeState(EGamePlatformAIError Error);
};
