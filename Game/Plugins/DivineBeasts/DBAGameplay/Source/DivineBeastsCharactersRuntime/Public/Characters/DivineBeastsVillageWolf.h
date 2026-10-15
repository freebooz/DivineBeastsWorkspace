// SSLM项目层双端狼怪物：唯一GAS/Combat权威生命，物种外观由客户端内容包注入。
#pragma once

#include "Characters/DivineBeastsCharacter.h"
#include "DivineBeastsVillageWolf.generated.h"

class UGamePlatformAIStateComponent;
struct FGamePlatformCombatEvent;

/**
 * 四足狼怪物的稳定玩法实体。服务器创建并初始化生命，由平台Combat结算伤害；
 * 只复制有限身份和死亡状态，绝不硬引用Fab骨骼、贴图、动画或客户端表现。
 * 怪物没有玩家身份和英雄技能，不参与竞技；AI只在Server端持有Controller。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSCHARACTERSRUNTIME_API ADivineBeastsVillageWolf : public ADivineBeastsCharacter
{
    GENERATED_BODY()
public:
    ADivineBeastsVillageWolf();

    /** 仅服务端游戏线程、已Possess且组件有效时初始化；失败绝不产生可攻击但不可受伤的怪物。 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="SSLM|VillageWolf")
    bool AuthorityInitializeWolf(float MaxHealth);

    /** 双端只读、由本Actor复制；死亡即使尚有客户端生命属性延迟也立即拒绝本地表现。 */
    UFUNCTION(BlueprintPure, Category="SSLM|VillageWolf")
    bool IsWolfAlive() const { return bWolfInitialized && !bWolfDead; }

    /** 中立平台AI状态组件，定义资源由服务器装配根配置，客户端只消费复制快照。 */
    UGamePlatformAIStateComponent* GetWolfAIState() const { return AIState; }

    /** 真实Combat是唯一伤害入口；本怪物源资格不读取不存在的英雄身份。 */
    virtual bool CanSourceCombat() const override;
    virtual bool CanReceiveCombat() const override;
    virtual int32 GetCombatAvatarGeneration() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    /** 平台复用AI状态/代次，仅项目实例配置物种参数；不依赖ServerOnly实现。 */
    UPROPERTY(VisibleAnywhere, Category="SSLM|VillageWolf")
    TObjectPtr<UGamePlatformAIStateComponent> AIState;

    /** 只有真实GAS/Combat重置成功才发布，避免异步登录、UI或旧死亡重复设置。 */
    UPROPERTY(Replicated)
    bool bWolfInitialized = false;

    /** 当前生命死亡权威事实，不在客户端自行修改Combat Health。 */
    UPROPERTY(ReplicatedUsing=OnRep_WolfDead)
    bool bWolfDead = false;

    FDelegateHandle DeathDelegate;
    bool bClosing = false;

    /** 真实Combat通知已确认死亡后调用；撤除胶囊碰撞避免尸体挡住路径。 */
    void HandleAuthorityDeath(const FGamePlatformCombatEvent& Event);
    UFUNCTION() void OnRep_WolfDead();
};
