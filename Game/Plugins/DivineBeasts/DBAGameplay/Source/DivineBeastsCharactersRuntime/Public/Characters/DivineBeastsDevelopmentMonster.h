// 项目双端开发训练敌人合同：复用ADivineBeastsCharacter的唯一ASC/Combat，不拥有玩家准入或表现资源。
#pragma once

#include "Characters/DivineBeastsCharacter.h"
#include "DivineBeastsDevelopmentMonster.generated.h"

struct FOnAttributeChangeData;

/** 游戏线程生命显示值快照；服务器仅从唯一Combat/GAS投影，客户端只读，不可用于授权或提交伤害。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsDevelopmentMonsterLifeState
{
    GENERATED_BODY()

    /** 当前GAS生命，单位生命点；未完成初始化为0。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|DevelopmentMonster")
    float Health = 0.0f;
    /** 当前GAS生命上限，单位生命点；开发训练基线为300，未初始化为0。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|DevelopmentMonster")
    float MaxHealth = 0.0f;
    /** 由Combat确认的死亡状态，不通过动画、可见性或Health setter自行判定权威死亡。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|DevelopmentMonster")
    bool bDead = false;
    /** 唯一Combat生命代次；0表示尚未发布有效生命，重生推进，不能当作玩家准入凭据。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|DevelopmentMonster")
    int32 AvatarGeneration = 0;
};

/** 每怪物实例的本地只读通知；参数借用本次值副本，同步重入不能将其改成后继生命。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDivineBeastsDevelopmentMonsterLifeStateChanged,
    const FDivineBeastsDevelopmentMonsterLifeState&);

/** 静止开发训练敌人；Actor拥有默认组件与生命监听，WorldSubsystem拥有生成/10秒重生/销毁策略。
 * 原生角色没有Mesh、材质或装饰BP路径；客户端独立装配可选表现，失败不改变Combat权威。
 */
UCLASS(NotBlueprintable)
class DIVINEBEASTSCHARACTERSRUNTIME_API ADivineBeastsDevelopmentMonster : public ADivineBeastsCharacter
{
    GENERATED_BODY()

public:
    ADivineBeastsDevelopmentMonster();

    /** 游戏线程读取唯一开发守卫：非Shipping/Test，GConfig许可且显式-DBADevelopmentMonsters。
     * 不缓存可变编辑器配置，不修改全局状态，不提供RPC或玩家授权；装配根另核实际世界/服务器体验。
     */
    static bool IsDevelopmentEnabledForCurrentProcess();

    /** 游戏线程只由权威开发装配调用；无Hero、无Controller，真实BeginPlay后初始化300生命。
     * 重复初始化、缺组件、开发关闭、非权威或退出返回false；不生成Actor，不伪造GAS写入回滚。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DivineBeasts|DevelopmentMonster")
    bool AuthorityInitializeDevelopmentMonster();

    /** 游戏线程只重生匹配ExpectedAvatarGeneration的已初始化死亡生命。
     * 零/旧代次、存活、开发关闭、通知重入或退出返回false；时间和安全出生位置由Server装配拥有。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DivineBeasts|DevelopmentMonster")
    bool AuthorityRespawnDevelopmentMonster(int32 ExpectedAvatarGeneration);

    /** 游戏线程按值返回当前完整显示快照；晚加入者读取复制状态，不依赖历史死亡多播。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|DevelopmentMonster")
    FDivineBeastsDevelopmentMonsterLifeState GetLifeState() const { return LifeState; }

    /** 游戏线程订阅本实例，消费者须以自己的Weak/句柄隔离来源并在退出解绑；本通知不跨网络。 */
    FDivineBeastsDevelopmentMonsterLifeStateChanged& OnLifeStateChanged() { return LifeStateChanged; }

    /** 稳定开发类型身份，不是Hero、玩家ID、Data租约或权威凭据。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|DevelopmentMonster")
    FName GetDevelopmentMonsterId() const;
    /** 可本地化只读显示名；不参与资源解析或权威身份比较。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|DevelopmentMonster")
    FText GetMonsterDisplayName() const;

    /** 开发训练敌人永不作为主动Combat来源，不能借初始化获得玩家/英雄技能资格。 */
    virtual bool CanSourceCombat() const override;
    /** 不使用玩家Hero Ready；仍要求真实唯一ASC绑定、有效本生命及Combat存活。 */
    virtual bool CanReceiveCombat() const override;
    /** Combat生命代次是此NPC唯一结算身份；不读取未绑定的英雄身份组件代次。 */
    virtual int32 GetCombatAvatarGeneration() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    /** 显示投影只由服务器读取Combat后发布，无独立加减生命入口或伤害公式。 */
    UPROPERTY(ReplicatedUsing=OnRep_LifeState)
    FDivineBeastsDevelopmentMonsterLifeState LifeState;
    /** 本实例非网络事件及GAS监听句柄，只释放自身订阅，不清其他观察者的GAS委托。 */
    FDivineBeastsDevelopmentMonsterLifeStateChanged LifeStateChanged;
    FDelegateHandle HealthChangedHandle;
    FDelegateHandle MaxHealthChangedHandle;
    /** 仅服务器初始化提交后置位；客户端有效生命从复制快照的正代次读取。 */
    bool bLifeInitialized = false;
    /** BeginPlay到EndPlay的单向关闭栅栏；同步监听者销毁Actor后原栈不得尾写。 */
    bool bMonsterClosing = false;
    /** 阻断真实GAS重置中的同步命令；在完整Combat提交前不开放受伤或显示中间状态。 */
    bool bLifeTransitionInProgress = false;
    /** 自有初始化/重生操作身份；跨GAS同步回调后必须仍匹配，退出首先失效。 */
    FGuid LifeTransitionId;
    /** 禁止在生命通知同步栈中重置Combat；普通后继伤害仍可发布独立值快照。 */
    int32 LifeNotificationDepth = 0;

    /** 初始化与重生共用GT操作边界；bRespawn决定死亡前提，失败不伪造GAS属性回滚。 */
    bool TransitionToNewLife(bool bRespawn, int32 ExpectedAvatarGeneration);
    /** 游戏线程仅采样实际Combat；初始化/重生中间值和死亡结算中间值不发布。 */
    void PublishCombatLifeState();
    /** 每次发布先拷贝值，在当前通知退栈前禁止重置Combat；后继伤害可以产生独立嵌套通知。 */
    void BroadcastLifeStateValue(const FDivineBeastsDevelopmentMonsterLifeState& State);
    /** 服务器属性事件只触发采样；客户端GAS属性到达不作为显示快照的第二个写入口。 */
    void HandleVitalAttributeChanged(const FOnAttributeChangeData& Change);
    /** 只接本Actor当前代次的真实Combat结果，死亡关闭胶囊后再采样完整事实。 */
    UFUNCTION() void HandleMonsterCombatEvent(const FGamePlatformCombatEvent& Event);
    /** 原子复制快照到达后发布本实例值通知；未初始化或关闭不再通知。 */
    UFUNCTION() void OnRep_LifeState();
};
