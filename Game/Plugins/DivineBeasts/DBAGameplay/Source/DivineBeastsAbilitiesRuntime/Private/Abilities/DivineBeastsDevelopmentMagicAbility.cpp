// 项目开发魔法数据适配：原反射/祖先兼容，开发资格不变，权威执行只有公共CombatGameplayAbility。
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
#include "Engine/AssetManager.h"

UGameplayEffect* UDivineBeastsDevelopmentMagicAbility::GetCostGameplayEffect() const
{
    // 不消耗气势，唯一GAS成本GE扣除默认能量；不退款、不绕过Commit。
    return BuildDefaultEnergyCost(nullptr);
}

void UDivineBeastsDevelopmentMagicAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 保留DevelopmentGameplayAbility反射祖先，但不执行它的即时射线样板。
    UDivineBeastsCombatGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UDivineBeastsDevelopmentMagicAbility::TryBuildCombatExecution(
    const FDivineBeastsAbilityBalanceRow& Balance,
    FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const
{
    const auto* Definition = Cast<UDivineBeastsDevelopmentMagicDefinition>(
        UAssetManager::Get().GetPrimaryAssetObject(AbilityDefinitionId));
    // 只读预热对象；开发开关和原生Definition验证保持原准入边界，不同步加载。
    if (!DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess() ||
        !Definition || !Definition->ValidateDefinition().IsSuccess() ||
        !FMath::IsNearlyEqual(Balance.CastRangeCm, Definition->ProjectileMotion.RangeCm))
    {
        OutError = TEXT("开发魔法未启用、未预热或定义运动/射程不合法。");
        return false;
    }
    OutExecution.Mode = EDivineBeastsCombatExecution::Projectile;
    OutExecution.Motion = Definition->ProjectileMotion;
    OutExecution.PresentationStyle = Definition->GetPresentationStyleId();
    OutExecution.ProjectileCount = Definition->ProjectileCount;
    OutExecution.ProjectileIntervalSeconds = Definition->ProjectileIntervalSeconds;
    OutExecution.CastDurationSeconds = Definition->MagicKind == EDivineBeastsDevelopmentMagicKind::Fireball ||
        Definition->MagicKind == EDivineBeastsDevelopmentMagicKind::Frostbolt ?
        UDivineBeastsDevelopmentMagicDefinition::DefaultCastDurationSeconds : 0.f;
    return true;
}
