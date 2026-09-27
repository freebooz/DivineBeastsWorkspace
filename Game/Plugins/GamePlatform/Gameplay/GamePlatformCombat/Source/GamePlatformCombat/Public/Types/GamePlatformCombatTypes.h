#pragma once

#include "CoreMinimal.h"
#include "GamePlatformCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformCombatEventType : uint8
{
    Damage,
    Healing,
    ControlApplied,
    ControlRemoved,
    Death,
    RespawnReset
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
