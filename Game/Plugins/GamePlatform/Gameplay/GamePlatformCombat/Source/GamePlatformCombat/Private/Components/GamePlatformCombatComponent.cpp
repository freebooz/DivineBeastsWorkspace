#include "Components/GamePlatformCombatComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Attributes/GamePlatformControlAttributeSet.h"
#include "Attributes/GamePlatformDefenseAttributeSet.h"
#include "Attributes/GamePlatformOffenseAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "Interfaces/GamePlatformCombatant.h"
#include "Net/UnrealNetwork.h"
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

    // 平台核心战斗属性集统一由权威 CombatComponent 确保存在；客户端通过 GAS 属性复制观察结果。
    if (GetOwner()->HasAuthority())
    {
        if (!AbilitySystemComponent->GetSet<UGamePlatformOffenseAttributeSet>())
        {
            AbilitySystemComponent->AddSet<UGamePlatformOffenseAttributeSet>();
        }
        if (!AbilitySystemComponent->GetSet<UGamePlatformDefenseAttributeSet>())
        {
            AbilitySystemComponent->AddSet<UGamePlatformDefenseAttributeSet>();
        }
        if (!AbilitySystemComponent->GetSet<UGamePlatformControlAttributeSet>())
        {
            AbilitySystemComponent->AddSet<UGamePlatformControlAttributeSet>();
        }
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

float UGamePlatformCombatComponent::GetCombatShield() const
{
    const UGamePlatformCombatAttributeSet* Attributes = GetCombatAttributeSet();
    return IsValid(Attributes) ? Attributes->GetShield() : 0.0f;
}

float UGamePlatformCombatComponent::GetCombatMaxShield() const
{
    const UGamePlatformCombatAttributeSet* Attributes = GetCombatAttributeSet();
    return IsValid(Attributes) ? Attributes->GetMaxShield() : 0.0f;
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

    const UGamePlatformControlAttributeSet* ControlAttributes =
        TargetASC->GetSet<UGamePlatformControlAttributeSet>();
    const float Tenacity = IsValid(ControlAttributes)
        ? ControlAttributes->GetTenacity()
        : 0.0f;
    const float FinalDuration = FGamePlatformCombatMath::CalculateControlDuration(
        DurationSeconds,
        Tenacity);

    if (FinalDuration <= KINDA_SMALL_NUMBER)
    {
        Result.FinalMagnitude = 0.0f;
        Result.ResultTags.AddTag(GamePlatformCombatTags::Result_ControlResisted);
        TargetComponent->PublishCombatEvent(
            EGamePlatformCombatEventType::ControlResisted,
            Spec,
            Result);
        MarkEventCompleted(Spec.EventId);
        return Result;
    }

    SpecHandle.Data->SetDuration(FinalDuration, true);
    const FActiveGameplayEffectHandle ActiveHandle =
        SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    if (!ActiveHandle.WasSuccessfullyApplied())
    {
        Result.Error = EGamePlatformCombatError::EffectApplicationFailed;
        return Result;
    }

    Result.FinalMagnitude = FinalDuration;
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
    float HealthFraction,
    float ShieldFraction)
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

    const float ClampedHealthFraction = FMath::Clamp(HealthFraction, 0.0f, 1.0f);
    const float ClampedShieldFraction = FMath::Clamp(ShieldFraction, 0.0f, 1.0f);

    AbilitySystemComponent->SetNumericAttributeBase(
        UGamePlatformCombatAttributeSet::GetHealthAttribute(),
        CombatAttributeSet->GetMaxHealth() * ClampedHealthFraction);

    AbilitySystemComponent->SetNumericAttributeBase(
        UGamePlatformCombatAttributeSet::GetShieldAttribute(),
        CombatAttributeSet->GetMaxShield() * ClampedShieldFraction);

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
    Result.RemainingShield = CombatAttributeSet->GetShield();

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

    FGamePlatformCombatResult Result =
        FGamePlatformCombatMath::ResolveDamage(
            Spec.EventId,
            Spec.Magnitude,
            FinalDamage,
            Attributes.GetShield(),
            Attributes.GetHealth(),
            bBypassShield);

    if (Spec.bCanCritical && IsValid(Spec.Source))
    {
        if (const UGamePlatformAbilitySystemComponent* SourceASC =
                Spec.Source->FindComponentByClass<UGamePlatformAbilitySystemComponent>())
        {
            if (const UGamePlatformOffenseAttributeSet* Offense =
                    SourceASC->GetSet<UGamePlatformOffenseAttributeSet>())
            {
                Result.bWasCritical = FGamePlatformCombatMath::IsCriticalHit(
                    true,
                    Offense->GetCriticalChance(),
                    FGamePlatformCombatMath::MakeDeterministicUnitRoll(Spec.EventId));
                if (Result.bWasCritical)
                {
                    Result.ResultTags.AddTag(GamePlatformCombatTags::Result_Critical);
                }
            }
        }
    }

    Attributes.SetShield(Result.RemainingShield);
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
        Attributes.GetMaxHealth(),
        Attributes.GetShield());

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
        const float MaxCoefficient = FMath::Max(0.0f, Settings->MaxDamageAttributeCoefficient);
        if (!FMath::IsFinite(Spec.AttackPowerCoefficient) ||
            !FMath::IsFinite(Spec.AbilityPowerCoefficient) ||
            Spec.AttackPowerCoefficient < 0.0f ||
            Spec.AbilityPowerCoefficient < 0.0f ||
            Spec.AttackPowerCoefficient > MaxCoefficient ||
            Spec.AbilityPowerCoefficient > MaxCoefficient)
        {
            return EGamePlatformCombatError::InvalidMagnitude;
        }

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
            GamePlatformCombatTags::Data_Damage_AttackPowerCoefficient,
            Spec.AttackPowerCoefficient);
        EffectSpec.Data->SetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_AbilityPowerCoefficient,
            Spec.AbilityPowerCoefficient);
        EffectSpec.Data->SetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_Type,
            static_cast<float>(Spec.DamageType));
        EffectSpec.Data->SetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_CanCritical,
            Spec.bCanCritical ? 1.0f : 0.0f);
        EffectSpec.Data->SetSetByCallerMagnitude(
            GamePlatformCombatTags::Data_Damage_CriticalRoll,
            FGamePlatformCombatMath::MakeDeterministicUnitRoll(Spec.EventId));
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
    Event.TargetActor = SourceSpec.Target;
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
