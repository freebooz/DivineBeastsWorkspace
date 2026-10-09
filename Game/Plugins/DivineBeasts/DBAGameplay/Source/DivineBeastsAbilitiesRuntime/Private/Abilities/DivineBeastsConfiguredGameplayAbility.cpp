#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

void UDivineBeastsConfiguredGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 任意新激活均重新开始提交资格；不得沿用上次施法的成本与冷却承诺。
    bCommittedForCurrentActivation = false;
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UDivineBeastsConfiguredGameplayAbility::CommitAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    FGameplayTagContainer* OptionalRelevantTags)
{
    // 引擎 GAS 负责 CheckCost、CheckCooldown、ApplyCost、ApplyCooldown 和网络预测，
    // 项目只记住“当前激活已成功提交”的事件，不复制成本/冷却计算。
    const bool bSucceeded = Super::CommitAbility(
        Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
    bCommittedForCurrentActivation = bSucceeded;
    return bSucceeded;
}

void UDivineBeastsConfiguredGameplayAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    bCommittedForCurrentActivation = false;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


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


bool UDivineBeastsConfiguredGameplayAbility::AuthorityTraceForwardAndApplyDamage(
    FGamePlatformCombatResult& OutResult, FString& OutError)
{
    OutResult = FGamePlatformCombatResult();

    // 客户端即使持有复制的Ability，也不能凭视觉/输入状态直接提交伤害。
    AActor* Source = GetAvatarActorFromActorInfo();
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    if (!IsActive() || !bCommittedForCurrentActivation ||
        !IsValid(Source) || !Source->HasAuthority() || !World)
    {
        OutError = TEXT("前向伤害查询必须在服务器已提交GAS技能后执行。");
        return false;
    }

    FDivineBeastsAbilityBalanceRow Balance;
    if (!TryReadConfiguredBalance(Balance, OutError))
    {
        return false;
    }
    if (!FMath::IsFinite(Balance.CastRangeCm) || Balance.CastRangeCm <= 0.0f ||
        !FMath::IsFinite(Balance.BaseDamage) || Balance.BaseDamage <= 0.0f)
    {
        OutError = TEXT("前向伤害技能没有合法施法距离或大于零的基础伤害。");
        return false;
    }

    // 先通过项目定义限制合法距离，再只在服务器当前World执行一次真实射线查询。
    // 通用平台层不理解生肖；此处不依赖MOBA或客户端Camera/VFX。
    const FVector TraceStart = Source->GetActorLocation() + FVector(0.0, 0.0, 50.0);
    const FVector Direction = Source->GetActorForwardVector().GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        OutError = TEXT("角色没有可用于技能命中的有效朝向。");
        return false;
    }
    const FVector TraceEnd = TraceStart + Direction * Balance.CastRangeCm;
    FCollisionQueryParams QueryParams(
        FName(TEXT("DivineBeastsAbilityForwardTrace")), false, Source);
    QueryParams.bReturnPhysicalMaterial = false;

    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(
            Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) ||
        !Hit.bBlockingHit || !IsValid(Hit.GetActor()) || Hit.GetActor() == Source)
    {
        OutError = TEXT("服务器前向技能射线未命中可执行伤害的有效目标。");
        return false;
    }

    const float Distance = FVector::Distance(TraceStart, Hit.ImpactPoint);
    if (!FMath::IsFinite(Distance) || Distance > Balance.CastRangeCm + 1.0f)
    {
        OutError = TEXT("服务器命中距离不合法，拒绝处理伤害。");
        return false;
    }

    FGamePlatformCombatHitContext HitContext;
    HitContext.bHasValidatedHit = true;
    HitContext.TraceStart = TraceStart;
    HitContext.TraceEnd = TraceEnd;
    HitContext.ImpactPoint = Hit.ImpactPoint;
    HitContext.ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
    HitContext.ValidatedDistance = Distance;

    // 只交给唯一GamePlatformCombat（平台战斗）组件做防御、护盾、血量与重复事件处理。
    return AuthorityApplyConfiguredDamage(Hit.GetActor(), HitContext, OutResult, OutError);
}

bool UDivineBeastsConfiguredGameplayAbility::AuthorityApplyConfiguredDamage(
    AActor* Target, const FGamePlatformCombatHitContext& ValidatedHit,
    FGamePlatformCombatResult& OutResult, FString& OutError)
{
    if (!IsActive() || !bCommittedForCurrentActivation)
    {
        OutError = TEXT("技能未处于有效激活或尚未成功提交 GAS 成本/冷却，拒绝造成伤害。");
        return false;
    }

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
    Spec.HitContext = ValidatedHit;
    Spec.SourceAbilityId = Balance.AbilityId;
    Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Spec.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration();

    // 技能仅提交已批准的基础伤害；Buff/Debuff、护盾GE及生命扣减均由平台战斗统一结算。
    OutResult = SourceCombat->ApplyDamage(Spec);
    if (OutResult.Error != EGamePlatformCombatError::None)
    {
        OutError = TEXT("战斗组件拒绝伤害请求，具体错误见平台 CombatResult.Error。");
        return false;
    }
    OutError.Reset();
    return true;
}
