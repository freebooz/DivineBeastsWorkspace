// 项目层服务器战斗执行：由兼容技能读取数据，统一拥有GAS激活与平台权威弹体的生命周期。
#pragma once

#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Types/GamePlatformCombatSpec.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "TimerManager.h"
#include "DivineBeastsCombatGameplayAbility.generated.h"

/** 数据适配选择已实现的服务器行为；不含最终生肖机制或客户端表现播放器。 */
enum class EDivineBeastsCombatExecution : uint8 { DirectDamage, Healing, Projectile };

/** 每次激活前读取一次的只读规则快照；来源是已预热Definition或兼容蓝图CDO，客户端不能填写。 */
struct FDivineBeastsCombatExecutionParameters
{
    /** 普攻射线、已批准治疗或平台投射物。 */
    EDivineBeastsCombatExecution Mode = EDivineBeastsCombatExecution::Projectile;
    /** 厘米/秒、厘米等单位遵循平台运动合同；只在Projectile模式使用。 */
    FGamePlatformCombatProjectileMotion Motion;
    /** 单次Commit释放1..3枚；每枚独立发生ID。 */
    int32 ProjectileCount = 1;
    /** 单发0，连发[0.05,1]秒；不包括首次起手。 */
    float ProjectileIntervalSeconds = 0.f;
    /** 首枚延迟秒；0表示立即释放，反馈窗口不自动变成权威延时。 */
    float CastDurationSeconds = 0.f;
    /** 仅复制中立表现语义，不加载Niagara或SFX。 */
    FName PresentationStyle = NAME_None;
};

/**
 * 服务器唯一的占位/开发魔法执行：统一Commit重入、冻结目标、起手/连发、生成和取消清理。
 * 适配类保留旧反射身份与数据字段；本类不持有另一套ASC、碰撞、伤害算法或表现播放器。
 * 游戏线程InstancedPerActor；每个激活令牌拒绝同步后继及迟到Timer，EndAbility先释放再通知。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsCombatGameplayAbility
    : public UDivineBeastsConfiguredGameplayAbility
{
    GENERATED_BODY()
public:
    /** 只在服务器执行，客户端经已有ASC提交输入。 */
    UDivineBeastsCombatGameplayAbility();
    /** 读取规则快照并冻结原目标，Commit成功且来源仍有效后启动唯一权威执行。 */
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    /** 先撤销自有Timer、弱目标和源强引用，再通知GAS；取消只撤销自己的施法事实。 */
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
    /** 适配层只提供数据/资格，失败不Commit；不得生成、计时、冻结目标或持有执行状态。 */
    virtual bool TryBuildCombatExecution(const FDivineBeastsAbilityBalanceRow& Balance,
        FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const;
private:
    /** 当前激活唯一所有权；任何同步回调返回后必须重新核对。 */
    FGuid ActivationToken;
    /** GC跟踪来源，终态清空；目标只保留代次及弱身份，禁止跨Timer强持有目标。 */
    UPROPERTY(Transient) FGamePlatformCombatSpec CastSpec;
    TWeakObjectPtr<AActor> FrozenProjectileTarget;
    /** 区别真正未选目标与原目标失效；后者必须取消，不能降级为直射。 */
    bool bHadFrozenTarget = false;
    /** Commit前冻结的数据快照；后续改选或CDO变化不修改本次在途规则。 */
    FDivineBeastsCombatExecutionParameters ActiveExecution;
    FName CastStyle = NAME_None;
    int32 RemainingShots = 0;
    int32 ReleasedShots = 0;
    /** 起手和逐枚连发共用一枚Timer，EndAbility统一清除。 */
    FTimerHandle BurstTimer;
    /** 每枚重新检查来源、世界和目标代次，再委托平台弹体执行运动/碰撞/结算。 */
    void ReleaseNextProjectile();
};
