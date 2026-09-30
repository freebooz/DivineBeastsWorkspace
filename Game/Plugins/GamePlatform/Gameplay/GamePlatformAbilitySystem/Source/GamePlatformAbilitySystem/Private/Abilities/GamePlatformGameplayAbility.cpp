#include "Abilities/GamePlatformGameplayAbility.h"
#include "Components/GamePlatformAbilitySystemComponent.h"

UGamePlatformGameplayAbility::UGamePlatformGameplayAbility()
{
    // AbilitySet 当前授权合同要求每个 Avatar Actor 拥有独立技能实例，
    // 避免把运行期状态错误共享到其他角色或其他世界实例。
    InstancingPolicy =
        EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UGamePlatformGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    check(IsInGameThread());
    const auto* Component = ActorInfo ? Cast<UGamePlatformAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
    if (!Component || !Component->EvaluateActivationEligibility().IsSuccess()) { return false; }
    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
