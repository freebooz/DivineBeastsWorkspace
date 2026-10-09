#pragma once

#include "CoreMinimal.h"
#include "GamePlatformCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformCombatEventType : uint8
{
    Damage,
    Healing,
    ControlApplied,
    ControlResisted, // 仅保留历史枚举序号兼容；已取消按数值韧性抵抗控制。
    ControlRemoved,
    Death,
    RespawnReset
};

/** EGamePlatformDamageType（跨游戏通用伤害类型）。 */
UENUM(BlueprintType)
enum class EGamePlatformDamageType : uint8
{
    /** 兼容旧路径：不走物理/魔法防御，但仍受通用DamageReduction影响。 */
    Untyped,
    /** 使用Armor（护甲）与ArmorPenetration（护甲穿透）。 */
    Physical,
    /** 使用MagicResistance（法术抗性）与MagicPenetration（法术穿透）。 */
    Magic,
    /** 忽略物理/魔法防御和通用DamageReduction。 */
    TrueDamage
};

UENUM(BlueprintType)
enum class EGamePlatformCombatError : uint8
{
    None,
    InvalidSource,
    InvalidTarget,
    InvalidMagnitude,
    NotAuthority,
    InvalidActorInfo,
    InvalidAvatarGeneration,
    TargetDead,
    SourceDead,
    HitValidationFailed,
    OutOfRange,
    Obstructed,
    SelfTargetNotAllowed,
    EffectApplicationFailed,
    ControlNotSupported,
    AlreadyDead,
    Cancelled
};

UENUM(BlueprintType)
enum class EGamePlatformControlType : uint8
{
    Stun,
    Silence
};
