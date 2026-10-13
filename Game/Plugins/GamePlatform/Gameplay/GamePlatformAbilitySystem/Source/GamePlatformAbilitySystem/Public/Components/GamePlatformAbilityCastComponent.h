// 平台技能实体组件：服务器发布/取消中立事实，Actor拥有复制与计时器；无项目/Combat/客户端依赖。
#pragma once
#include "Components/ActorComponent.h"
#include "Types/GamePlatformAbilityCastFacts.h"
#include "TimerManager.h"
#include "GamePlatformAbilityCastComponent.generated.h"

class UGamePlatformAbilityCastComponent;
/** 游戏线程通知；载荷是值快照，消费者允许同步取消/发布，必须按ActivationId隔离所有权。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGamePlatformAbilityCastChanged,
    UGamePlatformAbilityCastComponent*, const FGamePlatformAbilityCastFacts&);

/** 不提供网络命令RPC；仅可信服务器技能发布，客户端只订阅复制事实。组件不授予技能资格。 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformAbilityCastComponent final : public UActorComponent
{
    GENERATED_BODY()
public:
    UGamePlatformAbilityCastComponent();
    /** 游戏线程/权威/未退出，非空身份及名称、正代次、有限0..10秒时长才受理；同身份拒绝重置。
     * 原子替换旧窗口后通知；返回true只表示本次发布已受理，不保证重入消费者未取消。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="GamePlatform|Ability")
    bool PublishConfirmedCast(FGuid ActivationId, FName AbilityId, FName PresentationStyleId,
        int32 AvatarGeneration, float DurationSeconds);
    /** 仅权威且身份匹配的活跃窗口可撤销；旧身份、重复取消、客户端调用返回false且不影响新施法。 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="GamePlatform|Ability")
    bool CancelConfirmedCast(FGuid ActivationId);
    /** 返回值副本；客户端不得据此修改GAS成本、伤害或服务器时序。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Ability")
    FGamePlatformAbilityCastFacts GetCastFacts() const { return Facts; }
    FGamePlatformAbilityCastChanged& OnCastFactsChanged() { return Changed; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    /** 组件复制唯一真源；不使用普通委托冒充跨网络通知。 */
    UPROPERTY(ReplicatedUsing=OnRep_Facts) FGamePlatformAbilityCastFacts Facts;
    UFUNCTION() void OnRep_Facts();
    FGamePlatformAbilityCastChanged Changed;
    FTimerHandle ExpiryTimer;
    bool bClosing = false; // EndPlay后拒绝重入发布，不复活计时器。
};
