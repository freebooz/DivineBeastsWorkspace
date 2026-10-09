#include "Components/GamePlatformCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "Interfaces/GamePlatformCombatant.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Subsystems/GamePlatformCombatFeedbackWorldSubsystem.h"
#include "Settings/GamePlatformCombatSettings.h"
#include "Tags/GamePlatformAbilitySystemTags.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Types/GamePlatformCombatMath.h"

UGamePlatformCombatComponent::UGamePlatformCombatComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    AbilitySystemComponent =
        GetOwner()->FindComponentByClass<UGamePlatformAbilitySystemComponent>();

    if (!IsValid(AbilitySystemComponent))
    {
        return;
    }

    CombatAttributeSet =
        const_cast<UGamePlatformCombatAttributeSet*>(
            AbilitySystemComponent->GetSet<UGamePlatformCombatAttributeSet>());

    if (!IsValid(CombatAttributeSet) && GetOwner()->HasAuthority())
    {
        CombatAttributeSet =
            const_cast<UGamePlatformCombatAttributeSet*>(
                AbilitySystemComponent->AddSet<UGamePlatformCombatAttributeSet>());
    }

    AbilitySystemComponent->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Stun,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(
            this,
            &UGamePlatformCombatComponent::HandleControlTagChanged);

    AbilitySystemComponent->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Silence,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(
            this,
            &UGamePlatformCombatComponent::HandleControlTagChanged);
}

void UGamePlatformCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 玩家换Avatar或世界退出时主动释放本组件持有的独立限时护盾GE，
    // 不删除其他模块/装备授予的效果，避免跨世界残留虚构护盾容量。
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ClearShieldEffects();
    }
    ActiveShieldEffects.Reset();
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformCombatComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGamePlatformCombatComponent, bDead);
    DOREPLIFETIME(UGamePlatformCombatComponent, AvatarGeneration);
    DOREPLIFETIME(UGamePlatformCombatComponent, WorldContextGeneration);
}

UGamePlatformCombatAttributeSet* UGamePlatformCombatComponent::GetCombatAttributeSet() const
{
    if (IsValid(CombatAttributeSet))
    {
        return CombatAttributeSet;
    }

    if (!IsValid(AbilitySystemComponent))
    {
        return nullptr;
    }

    return const_cast<UGamePlatformCombatAttributeSet*>(
        AbilitySystemComponent->GetSet<UGamePlatformCombatAttributeSet>());
}

float UGamePlatformCombatComponent::GetCombatHealth() const
{
    const UGamePlatformCombatAttributeSet* Attributes = GetCombatAttributeSet();
    return IsValid(Attributes) ? Attributes->GetHealth() : 0.0f;
}

float UGamePlatformCombatComponent::GetCombatMaxHealth() const
{
    const UGamePlatformCombatAttributeSet* Attributes = GetCombatAttributeSet();
    return IsValid(Attributes) ? Attributes->GetMaxHealth() : 0.0f;
}

FGamePlatformCombatResult UGamePlatformCombatComponent::ApplyDamage(
    const FGamePlatformCombatSpec& InSpec)
{
    return ApplyInstantEffect(
        InSpec,
        UGamePlatformDamageGameplayEffect::StaticClass(),
        GamePlatformCombatTags::Data_Damage,
        false);
}

FGamePlatformCombatResult UGamePlatformCombatComponent::ApplyHealing(
    const FGamePlatformCombatSpec& InSpec)
{
    return ApplyInstantEffect(
        InSpec,
        UGamePlatformHealingGameplayEffect::StaticClass(),
        GamePlatformCombatTags::Data_Healing,
        true);
}

/**
 * GetAvailableShieldEffectCapacity（取得当前有效盾效果剩余容量）。
 * 仅服务端持有独立吸收账本；失效或已驱散的GE不能提供吸收值。
 * 客户端通过GameplayTag（效果标签）和一次性战斗反馈显示护盾，不同步盾容量属性。
 */
float UGamePlatformCombatComponent::GetAvailableShieldEffectCapacity() const
{
    if (!IsValid(AbilitySystemComponent))
    {
        return 0.0f;
    }

    double Total = 0.0;
    for (const FActiveShieldEffectCharge& Charge : ActiveShieldEffects)
    {
        if (Charge.EffectHandle.IsValid() &&
            AbilitySystemComponent->GetActiveGameplayEffect(Charge.EffectHandle) &&
            FMath::IsFinite(Charge.RemainingCapacity) &&
            Charge.RemainingCapacity > 0.0f)
        {
            Total += Charge.RemainingCapacity;
        }
    }
    return static_cast<float>(FMath::Clamp(Total, 0.0, 100000000.0));
}

void UGamePlatformCombatComponent::CompactExpiredShieldEffects()
{
    // 仅在护盾申请或战斗伤害发生时做有界清理，禁止每帧Tick。
    ActiveShieldEffects.RemoveAll([this](const FActiveShieldEffectCharge& Charge)
    {
        return !IsValid(AbilitySystemComponent) ||
            !Charge.EffectHandle.IsValid() ||
            !AbilitySystemComponent->GetActiveGameplayEffect(Charge.EffectHandle) ||
            !FMath::IsFinite(Charge.RemainingCapacity) ||
            Charge.RemainingCapacity <= KINDA_SMALL_NUMBER;
    });
}

void UGamePlatformCombatComponent::ConsumeShieldEffectCapacity(float RequestedAbsorption)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() ||
        !IsValid(AbilitySystemComponent) || !FMath::IsFinite(RequestedAbsorption))
    {
        return;
    }

    float Remaining = FMath::Max(0.0f, RequestedAbsorption);
    // 多来源效果按生效顺序消耗；每个GE都有自己的失效时刻和效果句柄。
    for (int32 Index = 0; Index < ActiveShieldEffects.Num() && Remaining > 0.0f;)
    {
        FActiveShieldEffectCharge& Charge = ActiveShieldEffects[Index];
        if (!AbilitySystemComponent->GetActiveGameplayEffect(Charge.EffectHandle))
        {
            ActiveShieldEffects.RemoveAt(Index, 1, EAllowShrinking::No);
            continue;
        }

        const float Used = FMath::Min(Charge.RemainingCapacity, Remaining);
        Charge.RemainingCapacity -= Used;
        Remaining -= Used;
        if (Charge.RemainingCapacity <= KINDA_SMALL_NUMBER)
        {
            const FActiveGameplayEffectHandle ConsumedHandle = Charge.EffectHandle;
            ActiveShieldEffects.RemoveAt(Index, 1, EAllowShrinking::No);
            // 盾容量耗尽即主动移除标签与效果，客户端现有Buff托盘可订阅标签变化。
            AbilitySystemComponent->RemoveActiveGameplayEffect(ConsumedHandle);
        }
        else
        {
            ++Index;
        }
    }
}

void UGamePlatformCombatComponent::ClearShieldEffects()
{
    // 先取走本组件拥有的句柄，再释放GAS效果，确保回调不会看到半更新账本。
    TArray<FActiveGameplayEffectHandle> Handles;
    Handles.Reserve(ActiveShieldEffects.Num());
    for (const FActiveShieldEffectCharge& Charge : ActiveShieldEffects)
    {
        if (Charge.EffectHandle.IsValid())
        {
            Handles.Add(Charge.EffectHandle);
        }
    }
    ActiveShieldEffects.Reset();
    if (IsValid(AbilitySystemComponent))
    {
        for (const FActiveGameplayEffectHandle& Handle : Handles)
        {
            AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
        }
    }
}

FGamePlatformCombatResult UGamePlatformCombatComponent::ApplyShield(
    const FGamePlatformCombatSpec& InSpec,
    float DurationSeconds)
{
    const FGamePlatformCombatSpec Spec = NormalizeSpec(InSpec);
    FGamePlatformCombatResult Result;
    Result.EventId = Spec.EventId;
    Result.RequestedMagnitude = Spec.Magnitude;

    if (HasCompletedEvent(Spec.EventId))
    {
        Result.Error = EGamePlatformCombatError::Cancelled;
        return Result;
    }

    // 复用伤害/治疗的服务器身份、Avatar代次、范围与正容量校验。
    // Shield允许对自己施加，但不允许客户端自行传入未获授权的效果结果。
    Result.Error = ValidateSpec(Spec, true, false);
    if (Result.Error != EGamePlatformCombatError::None)
    {
        return Result;
    }

    const UGamePlatformCombatSettings* Settings = GetDefault<UGamePlatformCombatSettings>();
    if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f ||
        DurationSeconds > Settings->MaxShieldDuration ||
        Spec.Magnitude > Settings->MaxShieldCapacity)
    {
        Result.Error = EGamePlatformCombatError::InvalidMagnitude;
        return Result;
    }

    UGamePlatformCombatComponent* TargetCombat =
        Spec.Target->FindComponentByClass<UGamePlatformCombatComponent>();
    UGamePlatformAbilitySystemComponent* SourceASC =
        Spec.Source->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    UGamePlatformAbilitySystemComponent* TargetASC =
        Spec.Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    if (!IsValid(TargetCombat) || !IsValid(SourceASC) || !IsValid(TargetASC))
    {
        Result.Error = EGamePlatformCombatError::InvalidActorInfo;
        return Result;
    }

    TargetCombat->CompactExpiredShieldEffects();
    if (TargetCombat->ActiveShieldEffects.Num() >= MaxActiveShieldEffects)
    {
        // 不用无界数组堆积护盾效果；拒绝第17个活跃盾，保持服务端内存可预测。
        Result.Error = EGamePlatformCombatError::InvalidMagnitude;
        return Result;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(Spec.Source, Spec.Source);
    FGameplayEffectSpecHandle EffectSpec =
        SourceASC->MakeOutgoingSpec(UGamePlatformShieldGameplayEffect::StaticClass(), 1.0f, Context);
    if (!EffectSpec.IsValid())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    EffectSpec.Data->SetDuration(DurationSeconds, true);
    const FActiveGameplayEffectHandle Handle =
        SourceASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);
    if (!Handle.WasSuccessfullyApplied() ||
        !TargetASC->GetActiveGameplayEffect(Handle))
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    FActiveShieldEffectCharge& Charge = TargetCombat->ActiveShieldEffects.AddDefaulted_GetRef();
    Charge.EffectHandle = Handle;
    Charge.RemainingCapacity = Spec.Magnitude;

    Result.Error = EGamePlatformCombatError::None;
    Result.FinalMagnitude = Spec.Magnitude;
    Result.RemainingShield = TargetCombat->GetAvailableShieldEffectCapacity();
    Result.RemainingHealth = TargetCombat->GetCombatHealth();
    MarkEventCompleted(Spec.EventId);
    return Result;
}

FGamePlatformCombatResult UGamePlatformCombatComponent::ApplyControl(
    const FGamePlatformCombatSpec& InSpec,
    EGamePlatformControlType ControlType,
    float DurationSeconds)
{
    FGamePlatformCombatSpec Spec = NormalizeSpec(InSpec);
    Spec.Magnitude = DurationSeconds;
    FGamePlatformCombatResult Result;
    Result.EventId = Spec.EventId;
    Result.RequestedMagnitude = DurationSeconds;

    if (HasCompletedEvent(Spec.EventId))
    {
        Result.Error = EGamePlatformCombatError::Cancelled;
        return Result;
    }

    const EGamePlatformCombatError Validation = ValidateSpec(Spec, false, false);
    if (Validation != EGamePlatformCombatError::None)
    {
        Result.Error = Validation;
        return Result;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();
    if (!FMath::IsFinite(DurationSeconds) ||
        DurationSeconds <= 0.0f ||
        DurationSeconds > Settings->MaxControlDuration)
    {
        Result.Error = EGamePlatformCombatError::InvalidMagnitude;
        return Result;
    }

    UGamePlatformCombatComponent* TargetComponent =
        Spec.Target->FindComponentByClass<UGamePlatformCombatComponent>();
    UGamePlatformAbilitySystemComponent* SourceASC =
        Spec.Source->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    UGamePlatformAbilitySystemComponent* TargetASC =
        Spec.Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();

    if (!IsValid(TargetComponent) || !IsValid(SourceASC) || !IsValid(TargetASC))
    {
        Result.Error = EGamePlatformCombatError::InvalidActorInfo;
        return Result;
    }

    TSubclassOf<UGameplayEffect> EffectClass;
    FGameplayTag ResultTag;
    switch (ControlType)
    {
    case EGamePlatformControlType::Stun:
        EffectClass = UGamePlatformStunGameplayEffect::StaticClass();
        ResultTag = GamePlatformCombatTags::Control_Stun;
        break;
    case EGamePlatformControlType::Silence:
        EffectClass = UGamePlatformSilenceGameplayEffect::StaticClass();
        ResultTag = GamePlatformCombatTags::Control_Silence;
        break;
    default:
        Result.Error = EGamePlatformCombatError::ControlNotSupported;
        return Result;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(Spec.Source, Spec.Source);
    FGameplayEffectSpecHandle SpecHandle =
        SourceASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
    if (!SpecHandle.IsValid())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    // 神兽联盟现行规则不包含控制韧性/失衡数值。服务器已在上文核验控制持续时间，
    // 直接应用已批准的技能时长；眩晕/沉默仍由GameplayEffect和GameplayTag负责。
    // 原有服务器权威、幂等和技能授权边界均保持，不能让客户端修改持续时间。
    SpecHandle.Data->SetDuration(DurationSeconds, true);
    const FActiveGameplayEffectHandle ActiveHandle =
        SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    if (!ActiveHandle.WasSuccessfullyApplied())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    Result.FinalMagnitude = DurationSeconds;
    Result.ResultTags.AddTag(ResultTag);
    TargetComponent->PublishCombatEvent(
        EGamePlatformCombatEventType::ControlApplied,
        Spec,
        Result);

    TargetComponent->ExecuteGameplayCue(
        GamePlatformCombatTags::GameplayCue_Control,
        Spec,
        Result);

    MarkEventCompleted(Spec.EventId);
    return Result;
}

bool UGamePlatformCombatComponent::RemoveControl(
    EGamePlatformControlType ControlType)
{
    if (!GetOwner()->HasAuthority() || !IsValid(AbilitySystemComponent))
    {
        return false;
    }

    FGameplayTag ControlTag;
    switch (ControlType)
    {
    case EGamePlatformControlType::Stun:
        ControlTag = GamePlatformCombatTags::Control_Stun;
        break;
    case EGamePlatformControlType::Silence:
        ControlTag = GamePlatformCombatTags::Control_Silence;
        break;
    default:
        return false;
    }

    FGameplayTagContainer Tags;
    Tags.AddTag(ControlTag);
    return AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(Tags) > 0;
}

bool UGamePlatformCombatComponent::ResetForNewAvatar(
    int32 NewAvatarGeneration,
    float HealthFraction)
{
    if (!GetOwner()->HasAuthority() ||
        !IsValid(AbilitySystemComponent) ||
        !IsValid(CombatAttributeSet))
    {
        return false;
    }

    if (NewAvatarGeneration <= AvatarGeneration)
    {
        NewAvatarGeneration = AvatarGeneration + 1;
    }

    FGameplayTagContainer CombatStateTags;
    CombatStateTags.AddTag(GamePlatformCombatTags::State_Dead);
    CombatStateTags.AddTag(GamePlatformCombatTags::Control_Stun);
    CombatStateTags.AddTag(GamePlatformCombatTags::Control_Silence);
    AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CombatStateTags);

    AbilitySystemComponent->RemoveLooseGameplayTag(
        GamePlatformCombatTags::State_Dead,
        1,
        EGameplayTagReplicationState::TagAndCountToAll);

    const float ClampedHealthFraction = FMath::IsFinite(HealthFraction)
        ? FMath::Clamp(HealthFraction, 0.0f, 1.0f)
        : 1.0f;

    // 重生不能承继旧化身的有限时护盾；先释放GE与其临时吸收实例。
    ClearShieldEffects();
    AbilitySystemComponent->SetNumericAttributeBase(
        UGamePlatformCombatAttributeSet::GetHealthAttribute(),
        CombatAttributeSet->GetMaxHealth() * ClampedHealthFraction);

    CombatAttributeSet->SetIncomingDamage(0.0f);
    CombatAttributeSet->SetIncomingHealing(0.0f);

    bDead = false;
    AvatarGeneration = NewAvatarGeneration;
    ++WorldContextGeneration;
    PendingResolutionStack.Reset();
    ResolvedResults.Reset();
    CompletedEventIds.Reset();
    CompletedEventOrder.Reset();

    FGamePlatformCombatSpec ResetSpec;
    ResetSpec.EventId = FGuid::NewGuid();
    ResetSpec.Source = GetOwner();
    ResetSpec.Target = GetOwner();
    ResetSpec.SourceAvatarGeneration = AvatarGeneration;
    ResetSpec.TargetAvatarGeneration = AvatarGeneration;

    FGamePlatformCombatResult Result;
    Result.EventId = ResetSpec.EventId;
    Result.RemainingHealth = CombatAttributeSet->GetHealth();

    PublishCombatEvent(
        EGamePlatformCombatEventType::RespawnReset,
        ResetSpec,
        Result);

    return true;
}

void UGamePlatformCombatComponent::ResolveIncomingDamage(
    UGamePlatformCombatAttributeSet& Attributes,
    const FGameplayEffectSpec& EffectSpec,
    float FinalDamage)
{
    if (!GetOwner()->HasAuthority() || FinalDamage <= 0.0f)
    {
        return;
    }

    const FGamePlatformCombatSpec Spec = GetPendingResolutionSpec(EffectSpec);
    if (!Spec.EventId.IsValid() || HasCompletedEvent(Spec.EventId))
    {
        return;
    }

    const bool bBypassShield =
        Spec.CombatTags.HasTag(GamePlatformCombatTags::Damage_BypassShield);

    // 统一结算顺序：GAS伤害增强与减免 → 限时护盾效果吸收 → 当前生命。
    // Shield是效果实例的临时容量，不是新的GAS属性或额外同步的角色数值。
    CompactExpiredShieldEffects();
    FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveDamage(
            Spec.EventId,
            Spec.Magnitude,
            FinalDamage,
            GetAvailableShieldEffectCapacity(),
            Attributes.GetHealth(),
            bBypassShield);

    if (Result.AppliedToShield > 0.0f)
    {
        ConsumeShieldEffectCapacity(Result.AppliedToShield);
    }
    Result.RemainingShield = GetAvailableShieldEffectCapacity();
    Attributes.SetHealth(Result.RemainingHealth);

    if (Result.bCausedDeath && !bDead)
    {
        EnterDeadState(Spec, Result);
    }

    ResolvedResults.Add(Spec.EventId, Result);
    MarkEventCompleted(Spec.EventId);

    PublishCombatEvent(
        EGamePlatformCombatEventType::Damage,
        Spec,
        Result);

    ExecuteGameplayCue(
        GamePlatformCombatTags::GameplayCue_Damage,
        Spec,
        Result);
}

void UGamePlatformCombatComponent::ResolveIncomingHealing(
    UGamePlatformCombatAttributeSet& Attributes,
    const FGameplayEffectSpec& EffectSpec,
    float FinalHealing)
{
    if (!GetOwner()->HasAuthority() || FinalHealing <= 0.0f)
    {
        return;
    }

    const FGamePlatformCombatSpec Spec = GetPendingResolutionSpec(EffectSpec);
    if (!Spec.EventId.IsValid() || HasCompletedEvent(Spec.EventId))
    {
        return;
    }

    FGamePlatformCombatResult Result;
    Result.EventId = Spec.EventId;
    Result.RequestedMagnitude = Spec.Magnitude;
    Result.FinalMagnitude = FinalHealing;

    if (bDead)
    {
        Result.Error = EGamePlatformCombatError::TargetDead;
        ResolvedResults.Add(Spec.EventId, Result);
        MarkEventCompleted(Spec.EventId);
        return;
    }

    Result = FGamePlatformCombatMath::ResolveHealing(
        Spec.EventId,
        Spec.Magnitude,
        FinalHealing,
        Attributes.GetHealth(),
        Attributes.GetMaxHealth());

    Result.RemainingShield = GetAvailableShieldEffectCapacity();
    Attributes.SetHealth(Result.RemainingHealth);

    ResolvedResults.Add(Spec.EventId, Result);
    MarkEventCompleted(Spec.EventId);

    PublishCombatEvent(
        EGamePlatformCombatEventType::Healing,
        Spec,
        Result);

    ExecuteGameplayCue(
        GamePlatformCombatTags::GameplayCue_Healing,
        Spec,
        Result);
}

void UGamePlatformCombatComponent::OnRep_Dead()
{
}

void UGamePlatformCombatComponent::HandleControlTagChanged(
    const FGameplayTag Tag,
    int32 NewCount)
{
    if (NewCount > 0)
    {
        return;
    }

    if (Tag != GamePlatformCombatTags::Control_Stun &&
        Tag != GamePlatformCombatTags::Control_Silence)
    {
        return;
    }

    FGamePlatformCombatSpec Spec;
    Spec.EventId = FGuid::NewGuid();
    Spec.Source = GetOwner();
    Spec.Target = GetOwner();
    Spec.SourceAvatarGeneration = AvatarGeneration;
    Spec.TargetAvatarGeneration = AvatarGeneration;

    FGamePlatformCombatResult Result;
    Result.EventId = Spec.EventId;
    Result.ResultTags.AddTag(Tag);

    PublishCombatEvent(
        EGamePlatformCombatEventType::ControlRemoved,
        Spec,
        Result);
}

EGamePlatformCombatError UGamePlatformCombatComponent::ValidateSpec(
    const FGamePlatformCombatSpec& Spec,
    bool bHealing,
    bool bCheckSelfPolicy) const
{
    if (!GetOwner()->HasAuthority())
    {
        return EGamePlatformCombatError::NotAuthority;
    }

    if (!IsValid(Spec.Source))
    {
        return EGamePlatformCombatError::InvalidSource;
    }

    if (Spec.Source != GetOwner())
    {
        return EGamePlatformCombatError::InvalidSource;
    }

    if (!IsValid(Spec.Target))
    {
        return EGamePlatformCombatError::InvalidTarget;
    }

    if (!FMath::IsFinite(Spec.Magnitude) || Spec.Magnitude <= 0.0f)
    {
        return EGamePlatformCombatError::InvalidMagnitude;
    }

    const UGamePlatformCombatSettings* Settings =
        GetDefault<UGamePlatformCombatSettings>();
    const float MagnitudeLimit =
        bHealing
            ? Settings->MaxHealingMagnitude
            : Settings->MaxDamageMagnitude;

    if (Spec.Magnitude > FMath::Max(0.0f, MagnitudeLimit))
    {
        return EGamePlatformCombatError::InvalidMagnitude;
    }

    if (!bHealing)
    {
        const int32 DamageTypeValue = static_cast<int32>(Spec.DamageType);
        if (DamageTypeValue < static_cast<int32>(EGamePlatformDamageType::Untyped) ||
            DamageTypeValue > static_cast<int32>(EGamePlatformDamageType::TrueDamage))
        {
            return EGamePlatformCombatError::InvalidMagnitude;
        }
    }

    if (Spec.Source->GetWorld() != Spec.Target->GetWorld())
    {
        return EGamePlatformCombatError::InvalidTarget;
    }

    UGamePlatformCombatComponent* SourceCombat =
        Spec.Source->FindComponentByClass<UGamePlatformCombatComponent>();
    UGamePlatformCombatComponent* TargetCombat =
        Spec.Target->FindComponentByClass<UGamePlatformCombatComponent>();

    UGamePlatformAbilitySystemComponent* SourceASC =
        Spec.Source->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    UGamePlatformAbilitySystemComponent* TargetASC =
        Spec.Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();

    if (!IsValid(SourceCombat) ||
        !IsValid(TargetCombat) ||
        !IsValid(SourceASC) ||
        !IsValid(TargetASC) ||
        !IsValid(SourceASC->GetOwnerActor()) ||
        !IsValid(SourceASC->GetAvatarActor()) ||
        !IsValid(TargetASC->GetOwnerActor()) ||
        !IsValid(TargetASC->GetAvatarActor()))
    {
        return EGamePlatformCombatError::InvalidActorInfo;
    }

    if (const IGamePlatformCombatant* SourcePolicy =
        Cast<IGamePlatformCombatant>(Spec.Source))
    {
        if (!SourcePolicy->CanSourceCombat())
        {
            return EGamePlatformCombatError::InvalidSource;
        }
    }

    if (const IGamePlatformCombatant* TargetPolicy =
        Cast<IGamePlatformCombatant>(Spec.Target))
    {
        if (!TargetPolicy->CanReceiveCombat())
        {
            return EGamePlatformCombatError::InvalidTarget;
        }
    }

    if (Spec.SourceAvatarGeneration != SourceCombat->AvatarGeneration ||
        Spec.TargetAvatarGeneration != TargetCombat->AvatarGeneration)
    {
        return EGamePlatformCombatError::InvalidAvatarGeneration;
    }

    if (SourceCombat->bDead)
    {
        return EGamePlatformCombatError::SourceDead;
    }

    if (TargetCombat->bDead)
    {
        return EGamePlatformCombatError::TargetDead;
    }

    if (bCheckSelfPolicy && Spec.Source == Spec.Target)
    {
        if (bHealing && !Settings->bAllowSelfHealing)
        {
            return EGamePlatformCombatError::SelfTargetNotAllowed;
        }

        if (!bHealing && !Settings->bAllowSelfDamage)
        {
            return EGamePlatformCombatError::SelfTargetNotAllowed;
        }
    }

    return EGamePlatformCombatError::None;
}

FGamePlatformCombatSpec UGamePlatformCombatComponent::NormalizeSpec(
    const FGamePlatformCombatSpec& InSpec) const
{
    FGamePlatformCombatSpec Spec = InSpec;
    if (!Spec.EventId.IsValid())
    {
        Spec.EventId = FGuid::NewGuid();
    }

    if (IsValid(Spec.Source))
    {
        if (UGamePlatformCombatComponent* SourceCombat =
            Spec.Source->FindComponentByClass<UGamePlatformCombatComponent>())
        {
            if (Spec.SourceAvatarGeneration <= 0)
            {
                Spec.SourceAvatarGeneration =
                    SourceCombat->GetCombatAvatarGeneration();
            }
        }
    }

    if (IsValid(Spec.Target))
    {
        if (UGamePlatformCombatComponent* TargetCombat =
            Spec.Target->FindComponentByClass<UGamePlatformCombatComponent>())
        {
            if (Spec.TargetAvatarGeneration <= 0)
            {
                Spec.TargetAvatarGeneration =
                    TargetCombat->GetCombatAvatarGeneration();
            }
        }
    }

    return Spec;
}

FGamePlatformCombatResult UGamePlatformCombatComponent::ApplyInstantEffect(
    const FGamePlatformCombatSpec& InSpec,
    TSubclassOf<UGameplayEffect> EffectClass,
    const FGameplayTag& MagnitudeTag,
    bool bHealing)
{
    const FGamePlatformCombatSpec Spec = NormalizeSpec(InSpec);

    FGamePlatformCombatResult Result;
    Result.EventId = Spec.EventId;
    Result.RequestedMagnitude = Spec.Magnitude;

    if (HasCompletedEvent(Spec.EventId))
    {
        Result.Error = EGamePlatformCombatError::Cancelled;
        return Result;
    }

    const EGamePlatformCombatError Validation = ValidateSpec(Spec, bHealing);
    if (Validation != EGamePlatformCombatError::None)
    {
        Result.Error = Validation;
        return Result;
    }

    UGamePlatformCombatComponent* TargetCombat =
        Spec.Target->FindComponentByClass<UGamePlatformCombatComponent>();
    UGamePlatformAbilitySystemComponent* SourceASC =
        Spec.Source->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    UGamePlatformAbilitySystemComponent* TargetASC =
        Spec.Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();

    if (!IsValid(TargetCombat) || !IsValid(SourceASC) || !IsValid(TargetASC))
    {
        Result.Error = EGamePlatformCombatError::InvalidActorInfo;
        return Result;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(Spec.Source, Spec.Source);

    FGameplayEffectSpecHandle EffectSpec =
        SourceASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
    if (!EffectSpec.IsValid())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    EffectSpec.Data->SetSetByCallerMagnitude(MagnitudeTag, Spec.Magnitude);
    EffectSpec.Data->AppendDynamicAssetTags(Spec.CombatTags);

    if (!bHealing)
    {
        EffectSpec.Data->SetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_Type,
            static_cast<float>(Spec.DamageType));
    }

    TargetCombat->PendingResolutionStack.Add(Spec);

    const FActiveGameplayEffectHandle Applied =
        SourceASC->ApplyGameplayEffectSpecToTarget(
            *EffectSpec.Data.Get(),
            TargetASC);

    if (!TargetCombat->PendingResolutionStack.IsEmpty())
    {
        TargetCombat->PendingResolutionStack.Pop(EAllowShrinking::No);
    }

    if (!Applied.WasSuccessfullyApplied())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    if (const FGamePlatformCombatResult* Resolved =
        TargetCombat->ResolvedResults.Find(Spec.EventId))
    {
        Result = *Resolved;
        TargetCombat->ResolvedResults.Remove(Spec.EventId);
        return Result;
    }

    Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
    return Result;
}

void UGamePlatformCombatComponent::MarkEventCompleted(const FGuid& EventId)
{
    if (!EventId.IsValid() || CompletedEventIds.Contains(EventId))
    {
        return;
    }

    CompletedEventIds.Add(EventId);
    CompletedEventOrder.Add(EventId);

    if (CompletedEventOrder.Num() > MaxRememberedEventIds)
    {
        const FGuid Oldest = CompletedEventOrder[0];
        CompletedEventOrder.RemoveAt(0, 1, EAllowShrinking::No);
        CompletedEventIds.Remove(Oldest);
    }
}

bool UGamePlatformCombatComponent::HasCompletedEvent(
    const FGuid& EventId) const
{
    return EventId.IsValid() && CompletedEventIds.Contains(EventId);
}

void UGamePlatformCombatComponent::EnterDeadState(
    const FGamePlatformCombatSpec& SourceSpec,
    FGamePlatformCombatResult& InOutResult)
{
    if (bDead || !GetOwner()->HasAuthority())
    {
        return;
    }

    bDead = true;
    InOutResult.bCausedDeath = true;
    // 死亡后旧GE盾立即消失，绝不继承到复活后的下一代Avatar。
    ClearShieldEffects();
    // 死亡事件和本次伤害结果也必须反映已经销毁的盾，避免客户端误显示残留保护值。
    InOutResult.RemainingShield = 0.0f;
    InOutResult.ResultTags.AddTag(GamePlatformCombatTags::State_Dead);

    if (IsValid(AbilitySystemComponent))
    {
        AbilitySystemComponent->AddLooseGameplayTag(
            GamePlatformCombatTags::State_Dead,
            1,
            EGameplayTagReplicationState::TagAndCountToAll);

        FGameplayTagContainer ActiveAbilityTags;
        ActiveAbilityTags.AddTag(GamePlatformAbilitySystemTags::Ability_Active);
        AbilitySystemComponent->CancelAbilities(
            &ActiveAbilityTags,
            nullptr,
            nullptr);

        FGameplayTagContainer ControlTags;
        ControlTags.AddTag(GamePlatformCombatTags::Control_Stun);
        ControlTags.AddTag(GamePlatformCombatTags::Control_Silence);
        AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(ControlTags);
    }

    PublishCombatEvent(
        EGamePlatformCombatEventType::Death,
        SourceSpec,
        InOutResult);

    ExecuteGameplayCue(
        GamePlatformCombatTags::GameplayCue_Death,
        SourceSpec,
        InOutResult);
}

void UGamePlatformCombatComponent::PublishCombatEvent(
    EGamePlatformCombatEventType EventType,
    const FGamePlatformCombatSpec& SourceSpec,
    const FGamePlatformCombatResult& Result)
{
    FGamePlatformCombatEvent Event;
    Event.EventId = Result.EventId;
    Event.EventType = EventType;
    Event.SourceActor = SourceSpec.Source;
    Event.TargetActor = SourceSpec.Target;    Event.SourceAbilityId = SourceSpec.SourceAbilityId;
    Event.SourceAvatarGeneration = SourceSpec.SourceAvatarGeneration;
    Event.TargetAvatarGeneration = SourceSpec.TargetAvatarGeneration;
    Event.RequestedMagnitude = Result.RequestedMagnitude;
    Event.AppliedMagnitude = Result.FinalMagnitude;
    Event.AppliedToShield = Result.AppliedToShield;
    Event.AppliedToHealth = Result.AppliedToHealth;
    Event.ResultTags = Result.ResultTags;
    Event.ImpactPoint = SourceSpec.HitContext.ImpactPoint;
    Event.ImpactNormal = SourceSpec.HitContext.ImpactNormal;
    Event.WorldContextGeneration = WorldContextGeneration;

    OnCombatEvent.Broadcast(Event);

    if (SourceSpec.Source != GetOwner() && IsValid(SourceSpec.Source))
    {
        if (UGamePlatformCombatComponent* SourceCombat =
            SourceSpec.Source->FindComponentByClass<UGamePlatformCombatComponent>())
        {
            SourceCombat->OnCombatEvent.Broadcast(Event);
        }
    }

    // 原有本地Delegate不具备网络传播能力；仅发送最小的服务器权威表现事实。
    // 真实数值仍由GAS属性复制，Unreliable丢包不允许造成结算丢失或重复扣血。
    if (GetOwner() && GetOwner()->HasAuthority() &&
        GetOwner()->GetIsReplicated())
    {
        FGamePlatformCombatFeedbackNetEvent Feedback;
        Feedback.EventId = Event.EventId;
        Feedback.EventType = Event.EventType;
        Feedback.SourceActor = Event.SourceActor;        Feedback.SourceAbilityId = Event.SourceAbilityId;
        Feedback.SourceAvatarGeneration = Event.SourceAvatarGeneration;
        Feedback.TargetAvatarGeneration = Event.TargetAvatarGeneration;
        Feedback.WorldContextGeneration = Event.WorldContextGeneration;
        Feedback.ImpactPoint = Event.ImpactPoint;
        Feedback.ImpactNormal = Event.ImpactNormal.GetSafeNormal();
        Feedback.AppliedMagnitude = Event.AppliedMagnitude;
        Feedback.AppliedToShield = Event.AppliedToShield;

        if (Feedback.IsSafeForCosmetics())
        {
            MulticastConfirmedCombatFeedback(Feedback);
        }
    }
}

void UGamePlatformCombatComponent::MulticastConfirmedCombatFeedback_Implementation(
    const FGamePlatformCombatFeedbackNetEvent& Feedback)
{
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() != NM_Client ||
        !Feedback.IsSafeForCosmetics() || !IsValid(GetOwner()) ||
        Feedback.TargetAvatarGeneration != AvatarGeneration ||
        Feedback.WorldContextGeneration != WorldContextGeneration)
    {
        // 主机/专服已有本地权威Delegate，不能在这里重复广播；旧角色/世界直接丢弃。
        return;
    }

    FGamePlatformCombatEvent Event;
    Event.EventId = Feedback.EventId;
    Event.EventType = Feedback.EventType;
    Event.SourceActor = Feedback.SourceActor;
    Event.TargetActor = GetOwner();    Event.SourceAbilityId = Feedback.SourceAbilityId;
    Event.SourceAvatarGeneration = Feedback.SourceAvatarGeneration;
    Event.TargetAvatarGeneration = Feedback.TargetAvatarGeneration;
    Event.WorldContextGeneration = Feedback.WorldContextGeneration;
    Event.AppliedMagnitude = Feedback.AppliedMagnitude;
    Event.AppliedToShield = Feedback.AppliedToShield;
    Event.AppliedToHealth = FMath::Max(0.0f,
        Feedback.AppliedMagnitude - Feedback.AppliedToShield);
    Event.ImpactPoint = Feedback.ImpactPoint;
    Event.ImpactNormal = Feedback.ImpactNormal;
    // 只投影为本World客户端的只读事实；不会执行客户端ApplyDamage。
    if (UGamePlatformCombatFeedbackWorldSubsystem* Bus =
        World->GetSubsystem<UGamePlatformCombatFeedbackWorldSubsystem>())
    {
        Bus->DispatchConfirmedFeedback(Event);
    }
}

void UGamePlatformCombatComponent::ExecuteGameplayCue(
    const FGameplayTag& CueTag,
    const FGamePlatformCombatSpec& SourceSpec,
    const FGamePlatformCombatResult& Result)
{
    if (!IsValid(AbilitySystemComponent) || !CueTag.IsValid())
    {
        return;
    }

    FGameplayCueParameters Parameters;
    Parameters.RawMagnitude = Result.FinalMagnitude;
    Parameters.Location = SourceSpec.HitContext.ImpactPoint;
    Parameters.Normal = SourceSpec.HitContext.ImpactNormal;
    Parameters.Instigator = SourceSpec.Source;
    Parameters.EffectCauser = SourceSpec.Source;

    AbilitySystemComponent->ExecuteGameplayCue(CueTag, Parameters);
}

FGamePlatformCombatSpec UGamePlatformCombatComponent::GetPendingResolutionSpec(
    const FGameplayEffectSpec& EffectSpec) const
{
    if (!PendingResolutionStack.IsEmpty())
    {
        return PendingResolutionStack.Last();
    }

    FGamePlatformCombatSpec Fallback;
    Fallback.EventId = FGuid::NewGuid();
    Fallback.Target = GetOwner();
    Fallback.TargetAvatarGeneration = AvatarGeneration;
    Fallback.EffectContext = EffectSpec.GetEffectContext();

    if (AActor* Instigator =
        EffectSpec.GetEffectContext().GetOriginalInstigator())
    {
        Fallback.Source = Instigator;
        if (UGamePlatformCombatComponent* SourceCombat =
            Instigator->FindComponentByClass<UGamePlatformCombatComponent>())
        {
            Fallback.SourceAvatarGeneration =
                SourceCombat->GetCombatAvatarGeneration();
        }
    }

    return Fallback;
}
