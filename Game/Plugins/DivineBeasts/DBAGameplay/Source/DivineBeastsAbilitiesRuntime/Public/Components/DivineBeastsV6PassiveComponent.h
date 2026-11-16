// SSLM V6固有被动服务器组件：事件驱动、无Tick、未签批资产不激活、只借用唯一ASC和Combat。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Definitions/DivineBeastsV6PassiveDefinition.h"
#include "Types/GamePlatformCombatEvent.h"
#include "DivineBeastsV6PassiveComponent.generated.h"

class UDivineBeastsCharacterComponent;
class UGamePlatformCombatComponent;
class UGamePlatformAbilitySystemComponent;
struct FGamePlatformCombatResult;

/**
 * 复用GAS持续效果叠层和复制；角色只提供已批准的被动配置，不拥有第二份伤害属性。
 * 未提供正式获批Definition时保持完全惰性，旧占位技能不触发本组件。
 */
UCLASS(ClassGroup=(DivineBeasts), meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsV6PassiveComponent final
    : public UActorComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsV6PassiveComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    /** 仅服务器在实际读取并验收正式DataService被动资产后显式调用；当前没有正式调用方。
     * bApprovedForRuntime默认false，合法数值及可信角色代次同时满足才可激活。
     */
    bool AuthorityConfigureFromApprovedDefinition(
        const UDivineBeastsV6PassiveDefinition& Definition, FString& OutError);

    /** V6权威伤害落地后调用，只有虎裂甲可真正作用于本次合法敌人。 */
    bool AuthorityOnConfirmedDamageHit(
        AActor* Target, int32 TargetAvatarGeneration,
        const FGamePlatformCombatResult& Result);

    /** 只读是否激活，不作为客户端授权依据。 */
    bool IsPassiveActive() const { return bPassiveActive; }

private:
    UFUNCTION()
    void HandleOwnerCombatEvent(const FGamePlatformCombatEvent& Event);
    void HandleCharacterReadinessChanged(bool bReady);
    void HandleCharacterIdentityChanged();
    void AuthorityDeactivate();

    bool IsCurrentOwnerContext() const;
    bool TryRememberEvent(FGuid EventId);

    TWeakObjectPtr<UDivineBeastsCharacterComponent> Identity;
    TWeakObjectPtr<UGamePlatformCombatComponent> Combat;
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;
    FDelegateHandle ReadinessHandle;
    FDelegateHandle IdentityHandle;
    bool bEndingPlay = false;
    bool bDeactivating = false;
    bool bPassiveActive = false;
    EDivineBeastsV6PassiveMode Mode = EDivineBeastsV6PassiveMode::Unsupported;
    FDivineBeastsV6PassiveRules Rules;
    FName ActiveHeroId = NAME_None;
    int32 ActiveAvatarGeneration = 0;
    FGuid ActiveTrustedOperation;

    /** 虎每次仅追踪同一猎物，换目标撤销上一个猎物上本人持有的GAS裂甲效果。 */
    TWeakObjectPtr<AActor> FractureTarget;
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> FractureTargetASC;
    int32 FractureTargetAvatarGeneration = 0;
    FActiveGameplayEffectHandle FractureEffect;

    /** 猪战意GE由自身ASC叠层，客户端经已有GAS属性复制观察伤害增量。 */
    FActiveGameplayEffectHandle BattleWillEffect;

    /** 同步战斗回调的EventId去重；至多存128条，不是全局记账/匹配历史。 */
    TSet<FGuid> SeenCombatEvents;
    TArray<FGuid> SeenCombatEventOrder;
};
