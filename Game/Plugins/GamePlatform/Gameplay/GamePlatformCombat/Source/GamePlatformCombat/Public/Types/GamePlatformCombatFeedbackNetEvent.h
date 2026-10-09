#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Types/GamePlatformCombatTypes.h"
#include "GamePlatformCombatFeedbackNetEvent.generated.h"

class AActor;

/**
 * FGamePlatformCombatFeedbackNetEvent（战斗表现网络事实）。
 *
 * 只传递服务器确认的最小命中信息，不包含可由客户端提交的伤害请求、
 * 权威生命值、技能授权或库存。由已有复制CombatComponent单向发送到相关客户端；
 * 丢包只损失一次可选视觉反馈，不改变GAS已结算结果。
 */
USTRUCT()
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatFeedbackNetEvent
{
    GENERATED_BODY()

    /** 全世界内单次权威结算的稳定GUID，客户端以此去重。 */
    UPROPERTY()
    FGuid EventId;

    /** 权威攻击者，网络不可见时可为空，受击者始终取接收组件Owner。 */
    UPROPERTY()
    TObjectPtr<AActor> SourceActor = nullptr;

    /** 可信战斗Spec提供的技能身份，不传输客户端可伪造的伤害。 */
    UPROPERTY()
    FName SourceAbilityId = NAME_None;

    /** 损伤、治疗、控制或死亡，禁止网络收包时重新结算。 */
    UPROPERTY()
    EGamePlatformCombatEventType EventType = EGamePlatformCombatEventType::Damage;

    /** 角色重生代次，用于过滤旧角色的迟到反馈。 */
    UPROPERTY()
    int32 SourceAvatarGeneration = 0;

    UPROPERTY()
    int32 TargetAvatarGeneration = 0;

    /** 跨世界会话代次，用于过滤旧世界网络事件。 */
    UPROPERTY()
    int32 WorldContextGeneration = 0;

    /** 压缩坐标；接触法线单位化由服务器负责。 */
    UPROPERTY()
    FVector_NetQuantize10 ImpactPoint = FVector::ZeroVector;

    UPROPERTY()
    FVector_NetQuantizeNormal ImpactNormal = FVector::UpVector;

    /** 仅驱动客户端表现强度，不能回写GAS属性。 */
    UPROPERTY()
    float AppliedMagnitude = 0.0f;

    /** 单次限时护盾GE的吸收量，只用于视觉反馈；不复制盾剩余容量属性。 */
    UPROPERTY()
    float AppliedToShield = 0.0f;

    /** 拒绝非法GUID、代次、非有限数值和不需要表现的事件。 */
    bool IsSafeForCosmetics() const
    {
        return EventId.IsValid() && TargetAvatarGeneration > 0 &&
            WorldContextGeneration > 0 && FMath::IsFinite(AppliedMagnitude) &&
            FMath::IsFinite(AppliedToShield) &&
            AppliedMagnitude >= 0.0f && AppliedToShield >= 0.0f &&
            (EventType != EGamePlatformCombatEventType::Damage ||
             AppliedToShield <= AppliedMagnitude) &&
            (EventType == EGamePlatformCombatEventType::Damage ||
             EventType == EGamePlatformCombatEventType::Healing ||
             EventType == EGamePlatformCombatEventType::ControlApplied ||
             EventType == EGamePlatformCombatEventType::Death);
    }
};
