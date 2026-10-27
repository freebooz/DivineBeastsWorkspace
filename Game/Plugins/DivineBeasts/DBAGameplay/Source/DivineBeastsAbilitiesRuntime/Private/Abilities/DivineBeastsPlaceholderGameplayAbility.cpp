// SSLM已序列化占位技能兼容适配：保留CDO字段和准入，不再拥有权威施法状态机。
#include "Abilities/DivineBeastsPlaceholderGameplayAbility.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"

UDivineBeastsPlaceholderGameplayAbility::UDivineBeastsPlaceholderGameplayAbility()
{
    ProjectileMotion.SpeedCmPerSecond = UDivineBeastsDevelopmentMagicDefinition::DefaultProjectileSpeedCmPerSecond;
}

UGameplayEffect* UDivineBeastsPlaceholderGameplayAbility::GetCostGameplayEffect() const
{
    // 不退款、不补资源、不绕过Commit；只排除用户已明确取消的法术气势成本，原有冷却仍走GAS。
    if (PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Fireball ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Frostbolt ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::ArcaneMissiles ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Healing) return BuildDefaultEnergyCost(nullptr);
    return Super::GetCostGameplayEffect();
}

bool UDivineBeastsPlaceholderGameplayAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // 占位被动既不扣除GAS成本也不伪装为可主动释放；正式被动能力另行替换。
    return PlaceholderMode != EDivineBeastsPlaceholderAbilityMode::Passive &&
        Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

const FGameplayTagContainer* UDivineBeastsPlaceholderGameplayAbility::GetCooldownTags() const
{
    return &PlaceholderCooldownTags;
}

bool UDivineBeastsPlaceholderGameplayAbility::TryBuildCombatExecution(
    const FDivineBeastsAbilityBalanceRow& Balance,
    FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const
{
    switch (PlaceholderMode)
    {
    case EDivineBeastsPlaceholderAbilityMode::Primary:
        OutExecution.Mode = EDivineBeastsCombatExecution::DirectDamage;
        return true;
    case EDivineBeastsPlaceholderAbilityMode::Healing:
        OutExecution.Mode = EDivineBeastsCombatExecution::Healing;
        // 已批准治疗是10秒冷却、零气势，仍由唯一GAS成本GE消费能量。
        return FMath::IsNearlyEqual(Balance.CooldownSeconds, 10.f) && Balance.MomentumCost == 0.f &&
            GetCooldownGameplayEffect() && !PlaceholderCooldownTags.IsEmpty();
    case EDivineBeastsPlaceholderAbilityMode::Fireball:
        OutExecution.PresentationStyle = TEXT("DBA.Development.Fireball"); break;
    case EDivineBeastsPlaceholderAbilityMode::Frostbolt:
        OutExecution.PresentationStyle = TEXT("DBA.Development.Frostbolt"); break;
    case EDivineBeastsPlaceholderAbilityMode::ArcaneMissiles:
        OutExecution.PresentationStyle = TEXT("DBA.Development.ArcaneMissiles"); break;
    default:
        OutError = TEXT("被动或未知占位类型不能主动执行。");
        return false;
    }
    OutExecution.Mode = EDivineBeastsCombatExecution::Projectile;
    OutExecution.Motion = ProjectileMotion;
    OutExecution.ProjectileCount = ProjectileCount;
    OutExecution.ProjectileIntervalSeconds = ProjectileIntervalSeconds;
    if (!PresentationStyleOverride.IsNone()) OutExecution.PresentationStyle = PresentationStyleOverride;
    OutExecution.CastDurationSeconds = PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Fireball ||
        PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Frostbolt ?
        UDivineBeastsDevelopmentMagicDefinition::DefaultCastDurationSeconds : 0.f;
    return true;
}
