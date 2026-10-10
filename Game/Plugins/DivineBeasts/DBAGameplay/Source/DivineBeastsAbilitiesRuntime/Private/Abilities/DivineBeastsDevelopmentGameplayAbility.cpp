// 神兽联盟开发验证执行器：服务端复用已审核的配置伤害路径，调用前必须具备开发进程资格。
#include "Abilities/DivineBeastsDevelopmentGameplayAbility.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"

UDivineBeastsDevelopmentGameplayAbility::UDivineBeastsDevelopmentGameplayAbility()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UDivineBeastsDevelopmentGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    FDivineBeastsAbilityBalanceRow Balance;
    FString Error;
    const AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess() ||
        !Avatar || !Avatar->HasAuthority() || !TryReadConfiguredBalance(Balance, Error) ||
        Balance.BaseDamage <= 0.0f || Balance.CastRangeCm <= 0.0f)
    {
        // 未实现的被动、治疗、护盾和位移不伪装成功，也不因外部意外激活产生费用或冷却。
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    FGamePlatformCombatResult Result;
    const bool bAppliedDamage = AuthorityTraceForwardAndApplyDamage(Result, Error);
    // 射线无目标或命中非战斗对象是正常失败，保留已提交的成本；不生成假的命中或固定成功结果。
    EndAbility(Handle, ActorInfo, ActivationInfo, true, !bAppliedDamage);
}

const FGameplayTagContainer* UDivineBeastsDevelopmentGameplayAbility::GetCooldownTags() const
{
    return &DevelopmentCooldownTags;
}

bool UDivineBeastsDevelopmentGameplayAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // 原生GAS的CanActivate与Commit都会调用此无副作用门禁，未实现候选不能在UI显示成可释放。
    // 蓝图CDO能力标记仅供客户端可用性；其无法替代上方服务器Activate的真实定义及数值校验。
    if (!DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess() ||
        !bDevelopmentDamageSampleSupported)
    {
        return false;
    }
    return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UDivineBeastsDevelopmentGameplayAbility::ApplyCooldown(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const UGameplayEffect* Effect = GetCooldownGameplayEffect();
    if (!Effect)
    {
        return; // 一级普通攻击/被动没有冷却GE，不另造零秒效果。
    }
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Effect->GetClass(), GetAbilityLevel());
    if (Spec.IsValid())
    {
        Spec.Data->DynamicGrantedTags.AppendTags(DevelopmentCooldownTags);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
    }
}
