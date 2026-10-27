#pragma once

#include "Abilities/DivineBeastsCombatGameplayAbility.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "DivineBeastsPlaceholderGameplayAbility.generated.h"

/** EDivineBeastsPlaceholderAbilityMode（临时技能执行类型），后期由正式技能类取代。 */
UENUM(BlueprintType)
enum class EDivineBeastsPlaceholderAbilityMode : uint8
{
    Primary,       // 普通攻击：权威前向单体伤害
    Passive,       // 被动：暂不实现，禁止主动释放
    Fireball,      // 火焰弹：默认0.5秒起手后释放单枚可选目标追踪弹体
    Frostbolt,     // 寒冰箭：默认0.5秒起手后释放单枚可选目标追踪弹体
    ArcaneMissiles, // 奥术飞弹：三枚螺旋连射弹体
    Healing        // 治疗：枚举值5，保留Primary及历史资产枚举值不迁号；输入由授权集决定。
};

/**
 * 临时生肖技能数据适配：旧反射身份及字段保留，权威执行统一归CombatGameplayAbility。
 * 这是获得用户明确批准的可替换占位技能，不依赖开发技能开关，不冒充十二生肖最终技能设计。
 * 行为类型由各英雄自己的蓝图CDO配置；各英雄独立AbilityId，不在这里推导生肖数值。
 * 无Niagara或UI依赖；客户端只显示服务器复制的Cast和Projectile事实。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsPlaceholderGameplayAbility
    : public UDivineBeastsCombatGameplayAbility
{
    GENERATED_BODY()

public:
    UDivineBeastsPlaceholderGameplayAbility();

    /** 普攻/被动/三类法术/治疗的行为；Primary资产保持稳定身份并显式迁移到Healing。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    EDivineBeastsPlaceholderAbilityMode PlaceholderMode =
        EDivineBeastsPlaceholderAbilityMode::Passive;

    /** 服务器恒速默认3000、允许3000..5000厘米/秒，客户端仅展示；射程与跟踪仍由平台权威弹体执行。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FGamePlatformCombatProjectileMotion ProjectileMotion;

    /** 项目纯内容表现语义覆盖；空值使用行为默认值，不改变弹道、伤害或服务器资格。
     * 客户端仍须通过精确语义/能力身份允许列表，不允许据此加载任意资源路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FName PresentationStyleOverride = NAME_None;

    /** 单次激活生成数量，火球/冰箭为1，奥术为3。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder",
        meta=(ClampMin="1", ClampMax="3"))
    int32 ProjectileCount = 1;

    /** 连射间隔，单发为0；不放宽平台投射物结算规则。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder",
        meta=(ClampMin="0", Units="s"))
    float ProjectileIntervalSeconds = 0.0f;

    /** 使用既有能力GAS授予的冷却标签，禁止自行在客户端计时。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Placeholder")
    FGameplayTagContainer PlaceholderCooldownTags;

    /** 三类占位法术及治疗使用默认能量GE，继续排除旧气势成本；普通攻击保持原配置。 */
    virtual UGameplayEffect* GetCostGameplayEffect() const override;
    virtual bool UsesDefaultSpellEnergy() const override
    { return PlaceholderMode != EDivineBeastsPlaceholderAbilityMode::Primary && PlaceholderMode != EDivineBeastsPlaceholderAbilityMode::Passive; }

    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual const FGameplayTagContainer* GetCooldownTags() const override;

protected:
    /** 旧蓝图反射身份/属性保留，只把CDO配置映射为唯一执行器的只读参数。 */
    virtual bool TryBuildCombatExecution(const FDivineBeastsAbilityBalanceRow& Balance,
        FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const override;
};
