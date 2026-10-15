// 项目开发GAS施法适配：继承平台技能资格/成本/冷却链，只生成通用平台投射物，不播放表现。
#pragma once
#include "Abilities/DivineBeastsDevelopmentGameplayAbility.h"
#include "Types/GamePlatformCombatSpec.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "TimerManager.h"
#include "DivineBeastsDevelopmentMagicAbility.generated.h"

/** 临时火焰/冰箭/奥术共用的服务器执行器；非Shipping/Test且配置/命令行显式启用。 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsDevelopmentMagicAbility : public UDivineBeastsDevelopmentGameplayAbility
{
    GENERATED_BODY()
public:
    /** 法术不消耗气势；GAS仍执行原有资格、冷却及Commit回调，不应用旧资产中的成本GE。 */
    virtual UGameplayEffect* GetCostGameplayEffect() const override;
    /** 读取已预热真实Definition，合法后只Commit一次；失败不制造弹体或成功日志。 */
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    /** 终态前清自有连发定时器及源强引用；已发射弹体不依赖技能对象继续存活，来源代次由平台核对。 */
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
    /** 仅当前GAS激活拥有，下一次激活先清；绝不从客户端RPC填充。 */
    UPROPERTY(Transient) FGamePlatformCombatSpec CastSpec;
    FGamePlatformCombatProjectileMotion Motion;
    FName StyleId = NAME_None;
    int32 RemainingProjectiles = 0, ReleasedProjectiles = 0;
    float IntervalSeconds = 0;
    FTimerHandle BurstTimer;
    /** 每次激活唯一令牌；Commit/Spawn同步回调取消并重新激活后，旧调用不得污染后继施法。 */
    FGuid CastActivationToken;
    /** 游戏线程连发；每枚重新核对激活/权威/来源存活，延期回调不会继承下一次施法资格。 */
    void ReleaseNextProjectile();
};
