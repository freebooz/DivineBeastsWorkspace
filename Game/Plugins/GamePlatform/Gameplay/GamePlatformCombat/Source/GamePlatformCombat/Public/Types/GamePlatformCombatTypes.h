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
    /** 兼容旧身份：无类型普通伤害也统一执行增伤-减伤。 */
    Untyped,
    /** Physical（物理）：只作已发布的技能与表现类型，统一增减伤计算。 */
    Physical,
    /** Magic（法术）：与物理共用DamageBonus/DamageReduction（增伤/减伤）公式。 */
    Magic,
    /** TrueDamage（真实伤害）：保留来源增伤但绕过目标普通减伤；限时GE盾仍可吸收，除非显式绕盾。 */
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
