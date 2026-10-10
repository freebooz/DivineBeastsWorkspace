// 神兽联盟双端运行角色实体：组合下层ASC/Combat/Gameplay资格，不拥有匹配、登录或出生策略。
#pragma once
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/GamePlatformCombatant.h"
#include "DivineBeastsCharacter.generated.h"

class UDivineBeastsCharacterComponent;
class UGamePlatformAbilitySystemComponent;
class UGamePlatformCombatComponent;
class UGamePlatformGameplayEligibilityComponent;
struct FGamePlatformCombatEvent;
/** 游戏线程服务器原生死亡事实；消费者借用事件值，只对目标Pawn本代次发布一次，不包含竞技规则。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsAuthoritativeDeath, const FGamePlatformCombatEvent&);

/** 默认子组件由Pawn拥有并随Pawn释放；拥有/复制事件绑定ActorInfo，未初始化或失去控制时失败关闭。 */
UCLASS()
class DIVINEBEASTSCHARACTERSRUNTIME_API ADivineBeastsCharacter : public ACharacter,
    public IAbilitySystemInterface, public IGamePlatformCombatant
{
    GENERATED_BODY()
public:
    ADivineBeastsCharacter();
    /** GAS通用查询返回本Pawn唯一ASC，调用方借用且不得自行释放。 */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual UGamePlatformAbilitySystemComponent* GetGamePlatformAbilitySystemComponent() const override;
    virtual UGamePlatformCombatComponent* GetGamePlatformCombatComponent() const override;
    /** 游戏线程读取资格与死亡事实；失败返回false，不产生权威副作用。 */
    virtual bool CanSourceCombat() const override;
    virtual bool CanReceiveCombat() const override;
    virtual int32 GetCombatAvatarGeneration() const override;
    /** 装配根订阅已结算的权威死亡；必须自行核世界/受信身份并在结束前解绑。客户端不会发布。 */
    FDivineBeastsAuthoritativeDeath& OnAuthoritativeDeath() { return AuthoritativeDeath; }
protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void OnRep_Controller() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FDivineBeastsAuthoritativeDeath AuthoritativeDeath;
    /** 本Pawn已经发布的死亡代次，0表示尚无；同代次重复Combat广播不重复终态。 */
    int32 PublishedDeathGeneration = 0;
    /** 双端游戏线程按当前拥有关系重新绑定；服务器失去拥有者时立即撤销Active。 */
    void RefreshAbilityActorInfo();
    /** 死亡权威事件立即失活；不靠客户端页面或每帧轮询执行玩法禁用。 */
    UFUNCTION() void HandleCombatEvent(const FGamePlatformCombatEvent& Event);
    /** Pawn拥有的唯一技能系统，GAS负责属性/能力复制；ActorInfo按真实拥有事件绑定。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;
    /** Pawn拥有的服务器战斗规则/复制状态，禁止客户端任意伤害RPC。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UGamePlatformCombatComponent> Combat;
    /** 独立竞技生命周期的最小权威资格；默认Inactive，不能由Widget/客户端写入。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UGamePlatformGameplayEligibilityComponent> GameplayEligibility;
    /** 项目可信身份/Definition自有租约和只读Ready，随Pawn结束。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;
};
