#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"

bool UDivineBeastsConfiguredGameplayAbility::TryReadConfiguredBalance(
    FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const
{
    if (!AbilityDefinitionId.IsValid() ||
        AbilityDefinitionId.PrimaryAssetType !=
            UGamePlatformPrimaryDataAsset::DefinitionAssetType())
    {
        OutError = TEXT("技能未配置有效的 GamePlatformDefinition 主资产身份。");
        return false;
    }

    const UDivineBeastsAbilityDefinition* Definition =
        Cast<UDivineBeastsAbilityDefinition>(
            UAssetManager::Get().GetPrimaryAssetObject(AbilityDefinitionId));
    if (!Definition || !Definition->ValidateDefinition().IsSuccess())
    {
        OutError = TEXT("技能玩法定义尚未预加载或未通过合法校验，拒绝施法。");
        return false;
    }

    const AActor* Source = GetAvatarActorFromActorInfo();
    const UDivineBeastsCharacterComponent* Character = IsValid(Source)
        ? Source->FindComponentByClass<UDivineBeastsCharacterComponent>()
        : nullptr;
    if (!Character || !Character->IsCharacterReady() ||
        Character->GetHeroDefinitionId() != Definition->HeroDefinitionId)
    {
        OutError = TEXT("当前技能所属英雄与可信角色身份不一致。");
        return false;
    }

    // GAS 技能等级是服务器授予的数值；Definition 只保存级别到表行的映射。
    return Definition->TryGetLoadedBalance(GetAbilityLevel(), OutRow, OutError);
}

bool UDivineBeastsConfiguredGameplayAbility::AuthorityApplyConfiguredDamage(
    AActor* Target, const FGamePlatformCombatHitContext& ValidatedHit,
    FGamePlatformCombatResult& OutResult, FString& OutError)
{
    AActor* Source = GetAvatarActorFromActorInfo();
    if (!IsValid(Source) || !Source->HasAuthority() ||
        !IsValid(Target) || !Target->HasAuthority() ||
        Source->GetWorld() != Target->GetWorld() || !ValidatedHit.bHasValidatedHit)
    {
        OutError = TEXT("伤害调用缺少服务器权威、同世界有效目标或可信命中事实。");
        return false;
    }

    FDivineBeastsAbilityBalanceRow Balance;
    if (!TryReadConfiguredBalance(Balance, OutError))
    {
        return false;
    }
    UGamePlatformCombatComponent* SourceCombat =
        Source->FindComponentByClass<UGamePlatformCombatComponent>();
    UGamePlatformCombatComponent* TargetCombat =
        Target->FindComponentByClass<UGamePlatformCombatComponent>();
    if (!SourceCombat || !TargetCombat)
    {
        OutError = TEXT("技能伤害缺少源或目标的平台战斗组件。");
        return false;
    }

    FGamePlatformCombatSpec Spec;
    Spec.EventId = FGuid::NewGuid();
    Spec.Source = Source;
    Spec.Target = Target;
    Spec.Magnitude = Balance.BaseDamage;
    Spec.DamageType = Balance.DamageType;
    Spec.AttackPowerCoefficient = Balance.AttackPowerCoefficient;
    Spec.AbilityPowerCoefficient = Balance.AbilityPowerCoefficient;
    Spec.bCanCritical = Balance.bCanCritical;
    Spec.HitContext = ValidatedHit;
    Spec.SourceAbilityId = Balance.AbilityId;
    Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Spec.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration();

    // 复用 GamePlatformCombat 的命中审查、攻击防御属性与一致性判定。
    OutResult = SourceCombat->ApplyDamage(Spec);
    if (OutResult.Error != EGamePlatformCombatError::None)
    {
        OutError = TEXT("战斗组件拒绝伤害请求，具体错误见平台 CombatResult.Error。");
        return false;
    }
    OutError.Reset();
    return true;
}
