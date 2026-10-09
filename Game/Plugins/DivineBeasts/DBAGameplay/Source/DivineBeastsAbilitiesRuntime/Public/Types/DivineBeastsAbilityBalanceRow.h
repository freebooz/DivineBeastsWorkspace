#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Types/GamePlatformCombatTypes.h"
#include "DivineBeastsAbilityBalanceRow.generated.h"

/**
 * FDivineBeastsAbilityBalanceRow（生肖技能等级数值表行）。
 * 存储服务端权威数值配置，不保存实时冷却或伤害结果。
 * AbilityId 为规范字符串（namespace.name@version），Level 以整数级数索引。
 * 所有距离单位为厘米，所有时间单位为秒。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSABILITIESRUNTIME_API FDivineBeastsAbilityBalanceRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 技能稳定逻辑编号；须与 Ability Definition 的 LogicalId 一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Identity")
    FName AbilityId = NAME_None;

    /** 该行对应的技能等级，范围 1～100。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Identity", meta=(ClampMin="1", ClampMax="100"))
    int32 Level = 1;

    /** 基础伤害及攻击/法术加成；只用于服务器已有 Combat 公式。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Damage", meta=(ClampMin="0"))
    float BaseDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Damage", meta=(ClampMin="0"))
    float AttackPowerCoefficient = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Damage", meta=(ClampMin="0"))
    float AbilityPowerCoefficient = 0.0f;

    /** 不在技能表中重新定义物理/法术/真实伤害算法。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Damage")
    EGamePlatformDamageType DamageType = EGamePlatformDamageType::Untyped;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Damage")
    bool bCanCritical = false;

    /** 技能冷却及气势成本；运行态分别由 GAS Effect 和 Momentum 属性集持有。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Cost", meta=(ClampMin="0"))
    float CooldownSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Cost", meta=(ClampMin="0"))
    float MomentumCost = 0.0f;

    /** 施法距离及有效区域半径，仅供权威命中/规则使用，视觉系统不得反向决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Targeting", meta=(ClampMin="0"))
    float CastRangeCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Ability|Targeting", meta=(ClampMin="0"))
    float AreaRadiusCm = 0.0f;

    /** 纯值检查，不访问世界或同步加载；失败给出可定位中文原因。 */
    bool Validate(FString& OutError) const;
};
