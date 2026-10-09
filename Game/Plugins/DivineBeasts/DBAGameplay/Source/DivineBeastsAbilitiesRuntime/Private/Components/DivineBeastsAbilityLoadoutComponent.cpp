#include "Components/DivineBeastsAbilityLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"

#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformAbilitySetDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "GameplayEffect.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ConfigCacheIni.h"

UDivineBeastsAbilityLoadoutComponent::UDivineBeastsAbilityLoadoutComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UDivineBeastsAbilityLoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* Owner = GetOwner();
    CharacterIdentity = Owner ? Owner->FindComponentByClass<UDivineBeastsCharacterComponent>() : nullptr;
    AbilitySystem = Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;

    // 角色加载按代次发就绪事件；技能授权必须晚于角色可信身份确认。
    if (UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get())
    {
        ReadinessHandle = Character->OnReadinessChanged().AddUObject(
            this, &UDivineBeastsAbilityLoadoutComponent::HandleCharacterReadinessChanged);
        if (Owner && Owner->HasAuthority() && Character->IsCharacterReady())
        {
            FString Error;
            AuthorityRefreshFromCharacter(Error);
        }
    }
}

void UDivineBeastsAbilityLoadoutComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get())
    {
        Character->OnReadinessChanged().Remove(ReadinessHandle);
    }
    ReadinessHandle.Reset();
    ++RequestSerial;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        RevokeOwnedGrants();
    }
    CharacterIdentity.Reset();
    AbilitySystem.Reset();
    Changed.Clear();
    Super::EndPlay(Reason);
}

void UDivineBeastsAbilityLoadoutComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(
        UDivineBeastsAbilityLoadoutComponent, LoadoutState, COND_OwnerOnly);
}

void UDivineBeastsAbilityLoadoutComponent::OnRep_LoadoutState()
{
    // 此处只通知本地 UI；不解析资源、不改变 ASC 的权威状态。
    Changed.Broadcast(LoadoutState);
}

IGamePlatformDataService* UDivineBeastsAbilityLoadoutComponent::FindDataService() const
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    return Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UDivineBeastsAbilityLoadoutComponent::HandleCharacterReadinessChanged(bool bReady)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }
    if (bReady)
    {
        FString Error;
        AuthorityRefreshFromCharacter(Error);
    }
    else
    {
        ++RequestSerial;
        RevokeOwnedGrants();
        PublishLoadout(NAME_None, 0, false, {});
    }
}

bool UDivineBeastsAbilityLoadoutComponent::AuthorityRefreshFromCharacter(FString& OutError)
{
    check(IsInGameThread());
    AActor* Owner = GetOwner();
    UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get();
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (!Owner || !Owner->HasAuthority() || !Character || !ASC ||
        !Character->IsCharacterReady())
    {
        OutError = TEXT("技能授权需要可信服务器角色身份、ASC及就绪状态。");
        return false;
    }

    const UDivineBeastsHeroDefinition* Hero = Character->GetLoadedDefinition();
    if (!Hero || Hero->DefinitionId != Character->GetHeroDefinitionId())
    {
        OutError = TEXT("当前英雄逻辑定义尚未完整加载或与出生身份不一致。");
        return false;
    }
    if (Hero->DefaultAbilitySetId.IsNone())
    {
        ++RequestSerial;
        RevokeOwnedGrants();
        PublishLoadout(Hero->DefinitionId, Character->GetAvatarGeneration(), false, {});
        OutError = TEXT("英雄尚未配置已批准的默认技能集合；禁止虚构技能。");
        return false;
    }

    if (LoadoutState.HeroDefinitionId == Hero->DefinitionId &&
        LoadoutState.AvatarGeneration == Character->GetAvatarGeneration() &&
        LoadoutState.bReady && PendingSetId == Hero->DefaultAbilitySetId)
    {
        OutError.Reset();
        return true;
    }
    // 同集合异步请求尚未完成时重复刷新是幂等的，不再重复 Acquire。
    if (DefinitionLease.IsValid() && PendingSetId == Hero->DefaultAbilitySetId &&
        LoadoutState.HeroDefinitionId == Hero->DefinitionId &&
        LoadoutState.AvatarGeneration == Character->GetAvatarGeneration())
    {
        OutError.Reset();
        return true;
    }

    ++RequestSerial;
    RevokeOwnedGrants();
    PublishLoadout(Hero->DefinitionId, Character->GetAvatarGeneration(), false, {});
    IGamePlatformDataService* Data = FindDataService();
    if (!Data)
    {
        OutError = TEXT("当前游戏实例未提供平台定义数据服务，拒绝技能授权。");
        return false;
    }

    PendingSetId = Hero->DefaultAbilitySetId;
    const int32 Serial = RequestSerial;
    const FName ExpectedHero = Hero->DefinitionId;
    const int32 ExpectedAvatar = Character->GetAvatarGeneration();
    const FPrimaryAssetId SetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(), PendingSetId);
    const TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> WeakSelf(this);
    FGamePlatformResult AcquireResult;
    DefinitionLease = Data->AcquireDefinition(
        SetId, UGamePlatformAbilitySetDefinition::StaticClass(),
        { TEXT("AbilitySet"), TEXT("Gameplay") }, EGamePlatformDataLifetime::World, this,
        [WeakSelf, Serial, ExpectedHero, ExpectedAvatar](
            const FGamePlatformDataLease& Lease, const FGamePlatformResult&)
        {
            if (UDivineBeastsAbilityLoadoutComponent* Self = WeakSelf.Get())
            {
                Self->HandleAbilitySetLoaded(Lease, Serial, ExpectedHero, ExpectedAvatar);
            }
        }, AcquireResult);
    if (!AcquireResult.IsSuccess() || !DefinitionLease.IsValid())
    {
        PendingSetId = NAME_None;
        OutError = TEXT("默认技能集合资源申请被平台数据服务拒绝。");
        return false;
    }
    OutError.Reset();
    return true;
}

void UDivineBeastsAbilityLoadoutComponent::HandleAbilitySetLoaded(
    const FGamePlatformDataLease& CompletedLease,
    int32 ExpectedRequestSerial, FName ExpectedHero, int32 ExpectedAvatar)
{
    check(IsInGameThread());
    UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get();
    IGamePlatformDataService* Data = FindDataService();
    if (!Data || !GetOwner() || !GetOwner()->HasAuthority() ||
        !DefinitionLease.IsValid() || !CompletedLease.IsValid() ||
        ExpectedRequestSerial != RequestSerial ||
        CompletedLease.LeaseId != DefinitionLease.LeaseId ||
        CompletedLease.Generation != DefinitionLease.Generation ||
        !Character || !Character->IsCharacterReady() ||
        Character->GetHeroDefinitionId() != ExpectedHero ||
        Character->GetAvatarGeneration() != ExpectedAvatar)
    {
        // 迟到回调不得安装上一个角色的技能，租约由最新请求/EndPlay清理。
        return;
    }
    if (Data->GetLeaseState(DefinitionLease) != EGamePlatformDataRequestState::Succeeded)
    {
        RevokeOwnedGrants();
        PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
        return;
    }

    const UGamePlatformAbilitySetDefinition* Set = Cast<UGamePlatformAbilitySetDefinition>(
        Data->GetLoadedDefinition(DefinitionLease));
    FString Error;
    if (!Set || !ApplyAbilitySet(*Set, Error))
    {
        RevokeOwnedGrants();
        PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
        return;
    }

    TArray<FDivineBeastsGrantedAbilitySlot> Slots;
    Slots.Reserve(Set->Abilities.Num());
    for (const FGamePlatformAbilityGrant& Grant : Set->Abilities)
    {
        FDivineBeastsGrantedAbilitySlot Slot;
        Slot.AbilityId = FName(*Grant.AbilityId.ToString());
        Slot.InputTag = Grant.InputTag;
        Slot.AbilityLevel = Grant.AbilityLevel;
        // 被动技能没有输入标签，使用稳定的内部独立槽位身份。
        Slot.SlotId = Grant.InputTag.IsValid()
            ? Grant.InputTag.GetTagName()
            : FName(*(FString(TEXT("Ability.Passive.")) + Grant.AbilityId.ToString()));
        Slots.Add(MoveTemp(Slot));
    }
    PublishLoadout(ExpectedHero, ExpectedAvatar, true, Slots);
}

bool UDivineBeastsAbilityLoadoutComponent::ApplyAbilitySet(
    const UGamePlatformAbilitySetDefinition& Set, FString& OutError)
{
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ASC ||
        !Set.ValidateDefinition().IsSuccess())
    {
        OutError = TEXT("未通过可信服务器技能集合校验。");
        return false;
    }
    // 正式客户端/专用服务器绝不能通过软引用意外加载并授予开发集合。
    // 仅在UE编辑器内、且配置显式开启后，允许开发样板参加PIE授权验证；
    // Shipping/Test及独立Client/Server Target始终拒绝，即使内容目录被误Cook。
    if (Set.bDevelopmentOnly)
    {
#if WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        bool bAllowDevelopmentAbilitySets = false;
        if (!GConfig || !GConfig->GetBool(
                TEXT("DivineBeasts.Abilities"),
                TEXT("bAllowDevelopmentAbilitySets"),
                bAllowDevelopmentAbilitySets, GGameIni) ||
            !bAllowDevelopmentAbilitySets)
        {
            OutError = TEXT("开发技能集合仅允许编辑器在显式启用测试开关后授予。");
            return false;
        }
#else
        OutError = TEXT("正式客户端和服务器目标禁止授予开发技能集合。");
        return false;
#endif
    }
    // 现阶段角色所需基础 AttributeSet 已由角色初始化提供；新增动态属性集
    // 一旦无法回滚将破坏授权事务，所以此实现拒绝它，而不是装作全部支持。
    if (!Set.Attributes.IsEmpty())
    {
        OutError = TEXT("当前默认技能集合尚不接受动态属性集授权，须使用正式属性初始化流程。");
        return false;
    }
    if (Set.Abilities.IsEmpty())
    {
        // 项目英雄默认技能集必须包含真实授予能力；不能将只含效果的集合伪装为可释放技能栏。
        OutError = TEXT("英雄默认技能集合缺少实际技能授权条目。");
        return false;
    }

    // 预检完整资源与当前 ASC 标签冲突；防止部分 GiveAbility 后失败留下半授权。
    TSet<FGameplayTag> NewInputTags;
    for (const FGamePlatformAbilityGrant& Grant : Set.Abilities)
    {
        UClass* AbilityClass = Grant.AbilityClass.Get();
        if (!AbilityClass || !AbilityClass->IsChildOf(UGamePlatformGameplayAbility::StaticClass()))
        {
            OutError = TEXT("技能类未按 AbilitySet Bundle 预加载或类型不合法。");
            return false;
        }
        // 平台约定一套ASC里同一个技能实现类不可重复授予；只检查既有真实授权，不按数组顺序覆盖。
        for (const FGameplayAbilitySpec& Existing : ASC->GetActivatableAbilities())
        {
            if (Existing.Ability && Existing.Ability->GetClass() == AbilityClass)
            {
                OutError = TEXT("当前角色已有相同技能实现类，拒绝重复授予。");
                return false;
            }
        }

        // 项目数据驱动技能必须绑定与 AbilitySet 相同的稳定 LogicalId，
        // 并确认数据依赖已由同一 DataService 租约预热，不能用同名图标或任意技能类冒充。
        const UGamePlatformGameplayAbility* AbilityCDO =
            AbilityClass->GetDefaultObject<UGamePlatformGameplayAbility>();
        if (!AbilityCDO)
        {
            OutError = TEXT("技能实现类缺少有效默认对象。");
            return false;
        }
        const UDivineBeastsConfiguredGameplayAbility* Configured =
            Cast<UDivineBeastsConfiguredGameplayAbility>(AbilityCDO);
        if (!Configured)
        {
            // 默认英雄技能必须由本项目数据驱动能力派生，防止跳过数值、气势与冷却门禁。
            OutError = TEXT("项目英雄技能未继承统一数据驱动技能基类。");
            return false;
        }
        else
        {
            const FPrimaryAssetId& DefinitionId = Configured->AbilityDefinitionId;
            if (!DefinitionId.IsValid() ||
                DefinitionId.PrimaryAssetType !=
                    UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
                DefinitionId.PrimaryAssetName != FName(*Grant.AbilityId.ToString()))
            {
                OutError = TEXT("技能行为绑定的主资产编号与能力集授权ID不一致。");
                return false;
            }
            const UDivineBeastsAbilityDefinition* Definition =
                Cast<UDivineBeastsAbilityDefinition>(
                    UAssetManager::Get().GetPrimaryAssetObject(DefinitionId));
            UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get();
            if (!Definition || !Definition->ValidateDefinition().IsSuccess() ||
                !Character || Definition->HeroDefinitionId != Character->GetHeroDefinitionId())
            {
                OutError = TEXT("数据驱动技能依赖尚未预热，或归属英雄与可信身份不一致。");
                return false;
            }
            FDivineBeastsAbilityBalanceRow Balance;
            if (!Definition->TryGetLoadedBalance(Grant.AbilityLevel, Balance, OutError))
            {
                return false;
            }
            // 静态策划表并不能自动扣气势：GAS Cost 必须是同等级实际负向瞬时修改。
            // 首批只接受可直接求值的 GE ScalableFloat/Curve Table；动态 SetByCaller
            // 必须先有独立已审核的成本协议，否则在 GiveAbility 前拒绝，不能默许数值漂移。
            const UGameplayEffect* CostEffect = Configured->GetCostGameplayEffect();
            float ActualMomentumChange = 0.0f;
            int32 MomentumModifiers = 0;
            if (CostEffect)
            {
                if (CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
                {
                    OutError = TEXT("技能气势成本 GameplayEffect 必须是瞬时效果，不允许周期或无限堆叠扣除。");
                    return false;
                }
                for (const FGameplayModifierInfo& Modifier : CostEffect->Modifiers)
                {
                    if (Modifier.Attribute !=
                        UDivineBeastsMomentumAttributeSet::GetMomentumAttribute())
                    {
                        // 其他资源需要扩充经过批准的数值字段和校验，不在此暗中加入成本。
                        OutError = TEXT("项目技能成本效果包含未在数值表声明的资源修改。");
                        return false;
                    }
                    float Change = 0.0f;
                    if (Modifier.ModifierOp != EGameplayModOp::Additive ||
                        !Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(
                            Grant.AbilityLevel, Change) || !FMath::IsFinite(Change))
                    {
                        OutError = TEXT("技能气势成本无法按真实技能等级计算静态负数，拒绝授权。");
                        return false;
                    }
                    ++MomentumModifiers;
                    ActualMomentumChange += Change;
                }
            }
            if ((Balance.MomentumCost > 0.0f && (!CostEffect || MomentumModifiers == 0)) ||
                !FMath::IsNearlyEqual(
                    ActualMomentumChange, -Balance.MomentumCost, 0.01f))
            {
                OutError = TEXT("技能数值表的气势成本与GAS实际瞬时扣除量不一致。");
                return false;
            }

            // 冷却的Duration也必须等于同等级策划表；存在效果却设置0秒同样不允许。
            const UGameplayEffect* CooldownEffect = Configured->GetCooldownGameplayEffect();
            if (Balance.CooldownSeconds > 0.0f || CooldownEffect)
            {
                float ActualCooldownSeconds = 0.0f;
                const FGameplayTagContainer* CooldownTags = Configured->GetCooldownTags();
                if (!CooldownEffect || !CooldownTags || CooldownTags->IsEmpty() ||
                    CooldownEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration ||
                    !CooldownEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(
                        Grant.AbilityLevel, ActualCooldownSeconds) ||
                    !FMath::IsFinite(ActualCooldownSeconds) ||
                    !FMath::IsNearlyEqual(
                        ActualCooldownSeconds, Balance.CooldownSeconds, 0.01f))
                {
                    OutError = TEXT("技能冷却标签、持续类型或同等级GE持续秒数与数值表不一致。");
                    return false;
                }
            }
        }
        if (Grant.InputTag.IsValid())
        {
            if (NewInputTags.Contains(Grant.InputTag))
            {
                OutError = TEXT("技能集合重复使用输入标签。");
                return false;
            }
            NewInputTags.Add(Grant.InputTag);
            for (const FGameplayAbilitySpec& Existing : ASC->GetActivatableAbilities())
            {
                if (Existing.GetDynamicSpecSourceTags().HasTagExact(Grant.InputTag))
                {
                    OutError = TEXT("当前角色已有其他来源的技能使用相同输入标签。");
                    return false;
                }
            }
        }
    }
    for (const FGamePlatformEffectGrant& Grant : Set.Effects)
    {
        if (!Grant.EffectClass.Get())
        {
            OutError = TEXT("启动 GameplayEffect 尚未预加载；拒绝部分授予。");
            return false;
        }
    }

    for (const FGamePlatformAbilityGrant& Grant : Set.Abilities)
    {
        FGameplayAbilitySpec Spec(Grant.AbilityClass.Get(), Grant.AbilityLevel);
        if (Grant.InputTag.IsValid())
        {
            Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
        }
        const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
        if (!Handle.IsValid())
        {
            OutError = TEXT("服务器 GAS 授予技能失败。");
            return false;
        }
        OwnedAbilityHandles.Add(Handle);
    }

    for (const FGamePlatformEffectGrant& Grant : Set.Effects)
    {
        const UGameplayEffect* Effect = Grant.EffectClass.Get()->GetDefaultObject<UGameplayEffect>();
        if (!Effect)
        {
            OutError = TEXT("启动效果默认对象无效。");
            return false;
        }
        const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
            Effect, Grant.Level, ASC->MakeEffectContext());
        if (!Handle.IsValid())
        {
            OutError = TEXT("启动效果应用失败，授予将回滚。");
            return false;
        }
        OwnedEffectHandles.Add(Handle);
    }
    OutError.Reset();
    return true;
}

void UDivineBeastsAbilityLoadoutComponent::RevokeOwnedGrants()
{
    IGamePlatformDataService* Data = FindDataService();
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (ASC && GetOwner() && GetOwner()->HasAuthority())
    {
        for (const FGameplayAbilitySpecHandle Handle : OwnedAbilityHandles)
        {
            if (Handle.IsValid())
            {
                ASC->CancelAbilityHandle(Handle);
                ASC->ClearAbility(Handle);
            }
        }
        for (const FActiveGameplayEffectHandle& Handle : OwnedEffectHandles)
        {
            if (Handle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(Handle);
            }
        }
    }
    OwnedAbilityHandles.Reset();
    OwnedEffectHandles.Reset();
    if (Data && DefinitionLease.IsValid())
    {
        Data->ReleaseDefinition(DefinitionLease);
    }
    DefinitionLease = {};
    PendingSetId = NAME_None;
}

void UDivineBeastsAbilityLoadoutComponent::PublishLoadout(
    FName HeroDefinitionId, int32 AvatarGeneration, bool bReady,
    const TArray<FDivineBeastsGrantedAbilitySlot>& Slots)
{
    LoadoutState.HeroDefinitionId = HeroDefinitionId;
    LoadoutState.AvatarGeneration = AvatarGeneration;
    if (LoadoutState.Revision < MAX_int64)
    {
        ++LoadoutState.Revision;
    }
    LoadoutState.bReady = bReady;
    LoadoutState.Slots = Slots;
    Changed.Broadcast(LoadoutState);
    if (AActor* Owner = GetOwner())
    {
        if (Owner->HasAuthority())
        {
            Owner->ForceNetUpdate();
        }
    }
}
