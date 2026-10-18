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

class APawn;
class AController;
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

    /** 游戏线程只读观察通知：死亡/化身复制或权威重置提交后发布，消费者自行读取当前公开状态。
     * 不提供写权限、不驱动伤害；订阅归消费者作用域，EndPlay前必须解绑，不依赖Tick检查旧化身。 */
    FSimpleMulticastDelegate& OnCombatObservationChanged() { return CombatObservationChanged; }

    /** 本地受控Pawn提交选中目标意图；true仅表示提交，服务器独立校验，不接受伤害/命中自报。
     * 与本Pawn的GAS请求共用Actor可靠通道；非战斗目标按清空处理，无Tick或自动寻敌。 */
    bool RequestProjectileTarget(AActor* Target);
    /** GT服务器选择目标；拒绝旧来源代次而不覆盖后继选择。空目标清除；其他非法目标清除旧选择。
     * 最大选择距离10000厘米仅限制意图，真正施法还须按技能射程和目标化身重新校验。 */
    bool AuthoritySelectProjectileTarget(AActor* Target,int32 ExpectedSourceGeneration,int32 ExpectedTargetGeneration);
    /** 服务器在施法命令中冻结一次有效选中目标；无效/超射程返回空并将输出代次归零。
     * 不复制选择列表、不查询UI、不跨世界引用；调用方不得把此查询当作伤害授权。 */
    AActor* GetSelectedProjectileTarget(float MaximumRangeCm,int32& OutTargetGeneration) const;

    /** GT只读瞄准点（世界厘米）：取当前启用碰撞的包围盒中心，无碰撞时退回Actor原点。
     * 施法起射、服务器追踪及确认命中表现共用；不依赖骨骼、Niagara或客户端模型资源。
     * 不提供命中资格或伤害授权，调用者仍须检查世界、存活与化身代次。 */
    FVector GetCombatTargetPoint() const;

    /**
     * GT服务器只读选中意图，不做存活/距离/化身过滤；true表示当前控制者曾明确选择目标。
     * 弱目标可因销毁/GC而为空，但双方冻结代次仍保留，治疗等策略可拒绝失效选择而不是误自疗。
     * 没有选择、换控制者或关闭作用域返回false并清空输出；调用者必须自行验证资格后才结算。
     */
    bool GetSelectedProjectileTargetIntent(TWeakObjectPtr<AActor>& OutTarget,
        int32& OutSourceGeneration, int32& OutTargetGeneration) const;

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

    /** GT服务器减速：数值比例0.05～0.70、持续1～10秒，真正降低GAS MoveSpeedMultiplier；
     * 目标必须持有可注册的ACharacterMovement和CombatAttributeSet。
     * 客户端只观察该属性复制；没有任意客户端施加减速RPC。 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    FGamePlatformCombatResult ApplySlow(
        const FGamePlatformCombatSpec& InSpec,
        float SlowFraction,
        float DurationSeconds);

    /** 平台中立方向计算：零分离、过高落差、不合法方向或速度均拒绝。 */
    static bool BuildDisplacementVelocity(
        const FVector& Epicenter,
        const FVector& TargetPosition,
        EGamePlatformDisplacementMode Mode,
        float HorizontalSpeedCmPerSecond,
        FVector& OutVelocity);

    /** GT服务器排队真实CharacterMovement移动，不将其伪装为传送或确定的落点。
     * Epicenter由权威施法逻辑传入；对目标身份、距离、世界、地面状态与近端胶囊路径失败关闭。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Displacement")
    FGamePlatformCombatResult ApplyForcedDisplacement(
        const FGamePlatformCombatSpec& InSpec,
        EGamePlatformDisplacementMode Mode,
        FVector Epicenter,
        float HorizontalSpeedCmPerSecond);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
    bool RemoveControl(EGamePlatformControlType ControlType);

    /**
     * GT服务器重置当前拥有者化身；HealthFraction为0～1生命比例，非有限值按1处理。
     * NewAvatarGeneration不大于当前值时按既有合同自动推进；int32代次耗尽拒绝且不改状态。
     * GAS同步通知可重入更高代次或关闭组件：被接管的旧调用返回false，不覆盖后继结果/需求；
     * 已完成的GAS写入不伪造回滚。只有本操作完整提交且结束通知后仍有效才返回true。
     */
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
    /** GT组件生命周期身份；BeginPlay建立新作用域，EndPlay先失效，旧同步栈不能复活。 */
    uint64 ComponentLifecycleGeneration = 0;
    /** 与复制Avatar值独立的重置操作身份；本次合法推进Avatar不会自行使操作失效。 */
    uint64 AvatarResetOperationGeneration = 0;
    /** EndPlay先置位且幂等；只有下一次引擎BeginPlay可建立新的开放作用域。 */
    bool bCombatClosing = false;
    /** 仅服务器持有的选择意图，换来源或目标化身即失效；具体弹体各自冻结，不跟随以后切换。 */
    TWeakObjectPtr<AActor> SelectedProjectileTarget;
    int32 SelectedProjectileSourceGeneration = 0;
    int32 SelectedProjectileTargetGeneration = 0;
    UFUNCTION(Server,Reliable)
    void ServerSelectProjectileTarget(AActor* Target,int32 ExpectedSourceGeneration,int32 ExpectedTargetGeneration);
    /** Pawn控制者生命周期通知；只清待施法选择，保留化身代次和已经冻结的弹体/连发。 */
    UFUNCTION()
    void HandleProjectileControllerChanged(APawn* ControlledPawn,AController* OldController,AController* NewController);

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystemComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformCombatAttributeSet> CombatAttributeSet = nullptr;

    UPROPERTY(ReplicatedUsing=OnRep_Dead)
    bool bDead = false;

    UPROPERTY(ReplicatedUsing=OnRep_AvatarGeneration)
    int32 AvatarGeneration = 1;

    UPROPERTY(Replicated)
    int32 WorldContextGeneration = 1;

    TArray<FGamePlatformCombatSpec> PendingResolutionStack;
    TMap<FGuid, FGamePlatformCombatResult> ResolvedResults;
    /** Slow的GAS GE应用会同步广播监听者：相同事件ID尚在应用中时，拒绝递归重复执行。 */
    TSet<FGuid> InFlightSlowEventIds;
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
    /** 逐个摘本批旧句柄后释放；未处理项保持拥有，后继Reset/EndPlay能接管清理，不清新盾。 */
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
    UFUNCTION()
    void OnRep_AvatarGeneration();
    /** 只读表现观察者不参与服务器结算；独立于可丢包CombatFeedback，不以远程特效事实推测死亡。 */
    FSimpleMulticastDelegate CombatObservationChanged;

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
