// 项目开发GAS施法适配：继承平台技能资格/成本/冷却链，只生成通用平台投射物，不播放表现。
#pragma once
#include "Abilities/DivineBeastsDevelopmentGameplayAbility.h"
#include "DivineBeastsDevelopmentMagicAbility.generated.h"

/** 已序列化开发魔法兼容入口：保留类路径、开发基类及蓝图身份，只读取Definition并委托唯一战斗执行。
 * 非Shipping/Test且配置/命令行显式启用；不能借公共基类跳过开发资格。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsDevelopmentMagicAbility : public UDivineBeastsDevelopmentGameplayAbility
{
    GENERATED_BODY()
public:
    /** 法术不消耗气势；GAS仍执行原有资格、冷却及Commit回调，不应用旧资产中的成本GE。 */
    virtual UGameplayEffect* GetCostGameplayEffect() const override;
    virtual bool UsesDefaultSpellEnergy() const override { return true; }
    /** 读取已预热真实Definition，合法后只Commit一次；火球/冰箭0.5秒起手结束才释放，失败不制造弹体。 */
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
protected:
    /** 读取预热Definition形成数据快照，失败不Commit，不拥有Timer/目标/生成状态。 */
    virtual bool TryBuildCombatExecution(const FDivineBeastsAbilityBalanceRow& Balance,
        FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const override;
};
