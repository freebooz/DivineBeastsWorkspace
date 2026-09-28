#include "Abilities/GamePlatformGameplayAbility.h"

UGamePlatformGameplayAbility::UGamePlatformGameplayAbility()
{
    // AbilitySet 当前授权合同要求每个 Avatar Actor 拥有独立技能实例，
    // 避免把运行期状态错误共享到其他角色或其他世界实例。
    InstancingPolicy =
        EGameplayAbilityInstancingPolicy::InstancedPerActor;
}
