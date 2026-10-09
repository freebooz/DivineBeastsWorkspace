#include "Components/GamePlatformAbilitySystemComponent.h"

#include "Abilities/GamePlatformGameplayAbility.h"

#include "GamePlatformAbilityTags.h"
#include "GameplayAbilitySpec.h"

UGamePlatformAbilitySystemComponent::UGamePlatformAbilitySystemComponent()
{
    SetIsReplicatedByDefault(true);
    // 输入作用域只在本组件实例内使用；每个ASC独立生成，既不复制也不持久化。
    InputScopeId = FGuid::NewGuid();
    // Mixed（混合复制）是多人玩家ASC的通用默认值：Owner接收完整GameplayEffect，其他客户端只接收必要Cue/Tag。
    // AI或纯服务端派生ASC可按场景进一步设置为Minimal，避免平台层把所有实例锁死为Full高带宽模式。
    SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UGamePlatformAbilitySystemComponent::OnRep_ActivateAbilities()
{
    // 只在 UE 原生技能授权容器完成增量复制后通知消费者。
    // UI 不能从旧的自定义 Loadout 快照推断 GAS Spec 已经同时到达。
    Super::OnRep_ActivateAbilities();
    AbilitySpecListChanged.Broadcast();
}

bool UGamePlatformAbilitySystemComponent::BindAbilityActorInfo(
    AActor* InOwnerActor,
    AActor* InAvatarActor)
{
    if (!IsValid(InOwnerActor) || !IsValid(InAvatarActor))
    {
        return false;
    }

    if (AbilityActorInfo.IsValid() &&
        AbilityActorInfo->OwnerActor.Get() == InOwnerActor &&
        AbilityActorInfo->AvatarActor.Get() == InAvatarActor)
    {
        return true;
    }

    // Avatar或Owner发生变化前先释放旧输入，避免换Pawn后“按住中”的Spec继续残留。
    ClearAbilityInput();
    InitAbilityActorInfo(InOwnerActor, InAvatarActor);
    ++AvatarGeneration;
    BroadcastAvatarBinding();
    return true;
}

void UGamePlatformAbilitySystemComponent::ClearAbilityAvatar()
{
    if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    ClearAbilityInput();
    ClearActorInfo();
    ++AvatarGeneration;
    BroadcastAvatarBinding();
}

FGamePlatformAbilityInputToken UGamePlatformAbilitySystemComponent::GetInputToken() const
{
    check(IsInGameThread());
    FGamePlatformAbilityInputToken Token;
    if (AbilityActorInfo.IsValid() && AbilityActorInfo->AvatarActor.IsValid() && AvatarGeneration > 0)
    {
        Token.ScopeId = InputScopeId;
        Token.AvatarGeneration = AvatarGeneration;
    }
    return Token;
}

bool UGamePlatformAbilitySystemComponent::IsInputTokenCurrent(
    const FGamePlatformAbilityInputToken& Token) const
{
    return Token.IsValid() && Token.ScopeId == InputScopeId &&
        Token.AvatarGeneration == AvatarGeneration && AbilityActorInfo.IsValid() &&
        AbilityActorInfo->AvatarActor.IsValid() && AbilityActorInfo->IsLocallyControlled();
}

FGameplayAbilitySpec* UGamePlatformAbilitySystemComponent::FindAbilitySpecByInputTag(
    FGameplayTag Tag,
    FGamePlatformResult& OutResult)
{
    if (!Tag.IsValid() || Tag == GamePlatformAbilityTags::InputRoot ||
        !Tag.MatchesTag(GamePlatformAbilityTags::InputRoot))
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("AbilityInputTagInvalid"),
            TEXT("能力输入标签必须是Platform.Ability.Input的有效子标签。"));
        return nullptr;
    }

    FGameplayAbilitySpec* Matched = nullptr;
    for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
    {
        if (!Spec.GetDynamicSpecSourceTags().HasTagExact(Tag))
        {
            continue;
        }
        if (Matched)
        {
            OutResult = FGamePlatformResult::Failure(
                TEXT("AbilityInputTagAmbiguous"),
                TEXT("同一能力输入标签匹配多个技能Spec，平台拒绝按授予顺序任选目标。"));
            return nullptr;
        }
        Matched = &Spec;
    }

    if (!Matched)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("AbilityInputSpecMissing"),
            TEXT("当前ASC没有与输入标签精确匹配的已授予技能Spec。"));
        return nullptr;
    }

    OutResult = FGamePlatformResult::Success();
    return Matched;
}

FGamePlatformResult UGamePlatformAbilitySystemComponent::AbilityInputPressed(
    FGameplayTag Tag,
    const FGamePlatformAbilityInputToken& Token)
{
    check(IsInGameThread());
    if (!IsInputTokenCurrent(Token))
    {
        return FGamePlatformResult::Failure(
            TEXT("AbilityInputTokenStale"),
            TEXT("能力输入令牌不属于当前本地Avatar代次。"));
    }

    FGamePlatformResult Result;
    FGameplayAbilitySpec* Spec = FindAbilitySpecByInputTag(Tag, Result);
    if (!Spec)
    {
        return Result;
    }

    if (!Spec->InputPressed)
    {
        Spec->InputPressed = true;
        AbilitySpecInputPressed(*Spec);
        PressedInputHandles.AddUnique(Spec->Handle);
        HeldInputHandles.AddUnique(Spec->Handle);
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformAbilitySystemComponent::AbilityInputReleased(
    FGameplayTag Tag,
    const FGamePlatformAbilityInputToken& Token)
{
    check(IsInGameThread());
    if (!IsInputTokenCurrent(Token))
    {
        return FGamePlatformResult::Failure(
            TEXT("AbilityInputTokenStale"),
            TEXT("能力输入释放令牌不属于当前本地Avatar代次。"));
    }

    FGamePlatformResult Result;
    FGameplayAbilitySpec* Spec = FindAbilitySpecByInputTag(Tag, Result);
    if (!Spec)
    {
        return Result;
    }

    if (Spec->InputPressed)
    {
        Spec->InputPressed = false;
        AbilitySpecInputReleased(*Spec);
    }
    PressedInputHandles.Remove(Spec->Handle);
    HeldInputHandles.Remove(Spec->Handle);
    return FGamePlatformResult::Success();
}

void UGamePlatformAbilitySystemComponent::CompactInputHandles()
{
    PressedInputHandles.RemoveAll([this](const FGameplayAbilitySpecHandle& Handle)
    {
        return FindAbilitySpecFromHandle(Handle) == nullptr;
    });
    HeldInputHandles.RemoveAll([this](const FGameplayAbilitySpecHandle& Handle)
    {
        return FindAbilitySpecFromHandle(Handle) == nullptr;
    });
}

void UGamePlatformAbilitySystemComponent::ProcessAbilityInput()
{
    check(IsInGameThread());
    if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid() ||
        !AbilityActorInfo->IsLocallyControlled())
    {
        ClearAbilityInput();
        return;
    }

    CompactInputHandles();

    // 新按下的技能每次按压最多尝试一次；是否持续重试由平台能力CDO上的ActivationPolicy显式声明。
    for (const FGameplayAbilitySpecHandle& Handle : PressedInputHandles)
    {
        if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle); Spec && !Spec->IsActive())
        {
            TryActivateAbility(Handle);
        }
    }

    for (const FGameplayAbilitySpecHandle& Handle : HeldInputHandles)
    {
        if (PressedInputHandles.Contains(Handle))
        {
            continue;
        }
        FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
        const UGamePlatformGameplayAbility* Ability = Spec
            ? Cast<UGamePlatformGameplayAbility>(Spec->Ability)
            : nullptr;
        if (Spec && Ability &&
            Ability->ActivationPolicy == EGamePlatformAbilityActivationPolicy::WhileHeld &&
            !Spec->IsActive())
        {
            TryActivateAbility(Handle);
        }
    }
    PressedInputHandles.Reset();
}

void UGamePlatformAbilitySystemComponent::ClearAbilityInput()
{
    check(IsInGameThread());
    for (const FGameplayAbilitySpecHandle& Handle : HeldInputHandles)
    {
        if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle); Spec && Spec->InputPressed)
        {
            Spec->InputPressed = false;
            AbilitySpecInputReleased(*Spec);
        }
    }
    PressedInputHandles.Reset();
    HeldInputHandles.Reset();
}

void UGamePlatformAbilitySystemComponent::BroadcastAvatarBinding()
{
    AvatarBindingChanged.Broadcast(GetAvatarBindingSnapshot());
}
