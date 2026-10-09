#include "Components/GamePlatformAbilitySystemComponent.h"

#include "Abilities/GamePlatformGameplayAbility.h"

#include "GamePlatformAbilityTags.h"
#include "GameplayAbilitySpec.h"
#include "Activation/AbilityActivationPolicy.h"

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

    // 旧宿主可先调用原生InitAbilityActorInfo；首个平台注册仍必须签发正代次，不能误判为已完成绑定。
    if (AvatarGeneration > 0 && AbilityActorInfo.IsValid() &&
        AbilityActorInfo->OwnerActor.Get() == InOwnerActor &&
        AbilityActorInfo->AvatarActor.Get() == InAvatarActor)
    {
        return true;
    }

    // Avatar或Owner发生变化前先释放旧输入，避免换Pawn后“按住中”的Spec继续残留。
    ClearAbilityInput();
    ActivationGate.Reset(); ActivationGateOwner.Reset(); ActivationGateGeneration = 0;
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
    ActivationGate.Reset(); ActivationGateOwner.Reset(); ActivationGateGeneration = 0;
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

    const auto Eligibility = EvaluateActivationEligibility();
    if (!Eligibility.IsSuccess()) { return Eligibility; }

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
    // 输入Spec也可能来自旧调用方直接Grant的原生能力；输入真实激活路径同样必须先过项目Gate。
    if (!EvaluateActivationEligibility().IsSuccess()) { ClearAbilityInput(); return; }

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

FGamePlatformResult UGamePlatformAbilitySystemComponent::SetActivationGate(TWeakObjectPtr<UObject> Owner,
    TSharedRef<IGamePlatformAbilityActivationGate> Gate)
{
    check(IsInGameThread());
    const auto Binding = GetAvatarBindingSnapshot();
    if (bEvaluatingActivationGate || !Binding.bBound || !Owner.IsValid() || !GetWorld() ||
        Owner->GetWorld() != GetWorld() || (ActivationGateOwner.IsValid() && ActivationGateOwner != Owner))
    { return FGamePlatformResult::Failure(TEXT("ActivationGateRegistrationRejected"), TEXT("资格读取器必须在当前Avatar就绪后由同世界唯一所有者注入。")); }
    ActivationGateOwner = Owner; ActivationGate = Gate; ActivationGateGeneration = Binding.AvatarGeneration;
    return FGamePlatformResult::Success();
}

bool UGamePlatformAbilitySystemComponent::ClearActivationGate(TWeakObjectPtr<UObject> Owner)
{
    check(IsInGameThread());
    if (bEvaluatingActivationGate || !ActivationGate.IsValid() || ActivationGateOwner != Owner) { return false; }
    ActivationGate.Reset(); ActivationGateOwner.Reset(); ActivationGateGeneration = 0; return true;
}

FGamePlatformResult UGamePlatformAbilitySystemComponent::EvaluateActivationEligibility() const
{
    check(IsInGameThread());
    const auto Binding = GetAvatarBindingSnapshot();
    if (bEvaluatingActivationGate || !GamePlatformAbilityActivationPolicy::CanEvaluate(Binding.bBound,
        ActivationGate.IsValid() && ActivationGateOwner.IsValid() && ActivationGateOwner->GetWorld() == GetWorld(),
        Binding.AvatarGeneration, ActivationGateGeneration))
    { return FGamePlatformResult::Failure(TEXT("ActivationGateUnavailable"), TEXT("当前Avatar未注入有效玩法资格读取器。")); }
    const auto Gate = ActivationGate;
    const auto Owner = ActivationGateOwner;
    const int32 Generation = AvatarGeneration;
    TGuardValue<bool> QueryGuard(bEvaluatingActivationGate, true);
    const auto Result = Gate->Evaluate(*this);
    // 查询不应写ASC；仍核对回调后身份，避免组合根重入切Avatar后把旧成功解释为新授权。
    if (!Owner.IsValid() || Owner->GetWorld() != GetWorld() || Generation != AvatarGeneration ||
        ActivationGate != Gate || ActivationGateOwner != Owner || !GetAvatarBindingSnapshot().bBound)
    { return FGamePlatformResult::Failure(TEXT("ActivationGateStale"), TEXT("资格查询期间Avatar或所有者已失效。")); }
    return Result;
}
