#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"

#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"#include "Components/DivineBeastsCharacterComponent.h"

#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

bool UDivineBeastsAbilityBarViewModel::BindToLoadout(
    UDivineBeastsAbilityLoadoutComponent* InLoadout)
{
    UnbindFromLoadout();
    if (!IsValid(InLoadout))
    {
        return false;
    }

    Loadout = InLoadout;
    LoadoutHandle = InLoadout->OnLoadoutChanged().AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleLoadoutChanged);
    // 授权投影可能先于 GAS AbilitySpec 的网络复制到达：使用平台原生 Spec 就绪事件补偿，
    // 严禁仅凭复刻出来的 SlotId 将未知或尚未复制的技能标记为可释放。
    BindToNativeAbilityEvents();
    HandleLoadoutChanged(InLoadout->GetLoadoutStateRef());
    return true;
}

void UDivineBeastsAbilityBarViewModel::UnbindFromLoadout()
{
    UnbindFromNativeAbilityEvents();
    if (UDivineBeastsAbilityLoadoutComponent* Current = Loadout.Get())
    {
        Current->OnLoadoutChanged().Remove(LoadoutHandle);
    }
    LoadoutHandle.Reset();
    Loadout.Reset();
    ++LoadGeneration;
    ResetProfileLease();
    if (!Slots.IsEmpty())
    {
        Slots.Reset();
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}

void UDivineBeastsAbilityBarViewModel::BeginDestroy()
{
    UnbindFromLoadout();
    Super::BeginDestroy();
}

void UDivineBeastsAbilityBarViewModel::ResetProfileLease()
{
    if (ProfileHandle.IsValid())
    {
        ProfileHandle->CancelHandle();
        ProfileHandle.Reset();
    }
    LoadedProfile = nullptr;
    LoadedHeroId = NAME_None;
}

void UDivineBeastsAbilityBarViewModel::BindToNativeAbilityEvents()
{
    UDivineBeastsAbilityLoadoutComponent* Current = Loadout.Get();
    AActor* Owner = Current ? Current->GetOwner() : nullptr;
    UGamePlatformAbilitySystemComponent* ASC = Owner
        ? Owner->FindComponentByClass<UGamePlatformAbilitySystemComponent>()
        : nullptr;
    if (!ASC)
    {
        return;
    }

    AbilitySystem = ASC;
    UDivineBeastsCharacterComponent* Identity =
        Owner->FindComponentByClass<UDivineBeastsCharacterComponent>();
    CharacterIdentity = Identity;
    if (Identity)
    {
        CharacterReadinessHandle = Identity->OnReadinessChanged().AddUObject(
            this, &UDivineBeastsAbilityBarViewModel::HandleCharacterReadinessChanged);
    }

    AbilitySpecHandle = ASC->OnAbilitySpecListChanged().AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleAbilitySpecListChanged);
    AbilityAvatarHandle = ASC->OnAvatarBindingChanged().AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleAbilityAvatarBindingChanged);

    // 冷却和消耗由 GAS 生效/移除事件及项目属性复制推动，不启动游戏业务 Tick。
    GameplayEffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleGameplayEffectAdded);
    GameplayEffectRemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(
        this, &UDivineBeastsAbilityBarViewModel::HandleGameplayEffectRemoved);
    MomentumHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()).AddUObject(
            this, &UDivineBeastsAbilityBarViewModel::HandleMomentumChanged);

    StunTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsAbilityBarViewModel::HandleCombatTagChanged);
    SilenceTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::Control_Silence, EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsAbilityBarViewModel::HandleCombatTagChanged);
    DeadTagHandle = ASC->RegisterGameplayTagEvent(
        GamePlatformCombatTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(
            this, &UDivineBeastsAbilityBarViewModel::HandleCombatTagChanged);
}

void UDivineBeastsAbilityBarViewModel::UnbindFromNativeAbilityEvents()
{
    UGamePlatformAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (ASC)
    {
        ASC->OnAbilitySpecListChanged().Remove(AbilitySpecHandle);
        ASC->OnAvatarBindingChanged().Remove(AbilityAvatarHandle);
        ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(GameplayEffectAddedHandle);
        ASC->OnAnyGameplayEffectRemovedDelegate().Remove(GameplayEffectRemovedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(
            UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()).Remove(MomentumHandle);
        ASC->RegisterGameplayTagEvent(
            GamePlatformCombatTags::Control_Stun, EGameplayTagEventType::NewOrRemoved).Remove(StunTagHandle);
        ASC->RegisterGameplayTagEvent(
            GamePlatformCombatTags::Control_Silence, EGameplayTagEventType::NewOrRemoved).Remove(SilenceTagHandle);
        ASC->RegisterGameplayTagEvent(
            GamePlatformCombatTags::State_Dead, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagHandle);
    }

    if (UDivineBeastsCharacterComponent* Identity = CharacterIdentity.Get())
    {
        Identity->OnReadinessChanged().Remove(CharacterReadinessHandle);
    }
    CharacterIdentity.Reset();
    CharacterReadinessHandle.Reset();
    AbilitySystem.Reset();
    AbilitySpecHandle.Reset();
    AbilityAvatarHandle.Reset();
    GameplayEffectAddedHandle.Reset();
    GameplayEffectRemovedHandle.Reset();
    MomentumHandle.Reset();
    StunTagHandle.Reset();
    SilenceTagHandle.Reset();
    DeadTagHandle.Reset();
}

void UDivineBeastsAbilityBarViewModel::RefreshCurrentLoadout()
{
    if (UDivineBeastsAbilityLoadoutComponent* Current = Loadout.Get())
    {
        RefreshSlots(Current->GetLoadoutStateRef());
    }
}

void UDivineBeastsAbilityBarViewModel::HandleAbilitySpecListChanged()
{
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleAbilityAvatarBindingChanged(
    const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot)
{
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleGameplayEffectAdded(
    UAbilitySystemComponent* Source, const FGameplayEffectSpec& Effect,
    FActiveGameplayEffectHandle Handle)
{
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleGameplayEffectRemoved(
    const FActiveGameplayEffect& Effect)
{
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleMomentumChanged(
    const FOnAttributeChangeData& ChangeData)
{
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleCombatTagChanged(
    const FGameplayTag Tag, int32 NewCount)
{
    RefreshCurrentLoadout();
}
void UDivineBeastsAbilityBarViewModel::HandleCharacterReadinessChanged(bool bReady)
{
    // 客户端角色身份和 OwnerOnly 授权快照可能跨组件乱序到达。
    // 每次角色就绪变化后重新核对 HeroId/AvatarGeneration，不可继续显示旧英雄可用槽位。
    RefreshCurrentLoadout();
}

bool UDivineBeastsAbilityBarViewModel::TryResolveGrantedSpec(
    UGamePlatformAbilitySystemComponent& ASC, FGameplayTag InputTag,
    const FGameplayAbilitySpec*& OutSpec)
{
    OutSpec = nullptr;
    if (!InputTag.IsValid())
    {
        return false;
    }
    for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
    {
        if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
        {
            continue;
        }
        if (OutSpec)
        {
            // 双技能占用同一输入标签属于歧义；按平台规则拒绝任意选用一个。
            OutSpec = nullptr;
            return false;
        }
        OutSpec = &Spec;
    }
    return OutSpec != nullptr && OutSpec->Ability != nullptr;
}

void UDivineBeastsAbilityBarViewModel::HandleLoadoutChanged(
    const FDivineBeastsAbilityLoadoutState& Snapshot)
{
    if (!Loadout.IsValid())
    {
        return;
    }
    if (Snapshot.HeroDefinitionId != LoadedHeroId)
    {
        ++LoadGeneration;
        ResetProfileLease();
        LoadedHeroId = Snapshot.HeroDefinitionId;
        if (!LoadedHeroId.IsNone())
        {
            const FPrimaryAssetId ProfileId(
                FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), LoadedHeroId);
            const int32 Serial = LoadGeneration;
            const FName Hero = LoadedHeroId;
            const FStreamableDelegate Completion = FStreamableDelegate::CreateUObject(
                this, &UDivineBeastsAbilityBarViewModel::HandleProfileLoaded, Serial, Hero);
            // 使用引擎统一 AssetManager（资产管理器）的已注册主资产，而不是猜测文件路径。
            // 缺失 Profile 时仍以真实服务器授权列表显示中性缺图标槽位。
            ProfileHandle = UAssetManager::Get().LoadPrimaryAsset(
                ProfileId, { FName(TEXT("UI")) }, Completion);
        }
    }
    RefreshSlots(Snapshot);
}

void UDivineBeastsAbilityBarViewModel::HandleProfileLoaded(
    int32 ExpectedLoadGeneration, FName ExpectedHero)
{
    if (ExpectedLoadGeneration != LoadGeneration ||
        ExpectedHero != LoadedHeroId || !Loadout.IsValid())
    {
        return;
    }
    const FPrimaryAssetId Id(
        FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), ExpectedHero);
    UDivineBeastsAbilityUIProfile* Profile = Cast<UDivineBeastsAbilityUIProfile>(
        UAssetManager::Get().GetPrimaryAssetObject(Id));
    FString Error;
    if (Profile && Profile->HeroDefinitionId == ExpectedHero &&
        Profile->ValidateProfile(Error))
    {
        LoadedProfile = Profile;
    }
    else
    {
        LoadedProfile = nullptr;
    }
    RefreshSlots(Loadout->GetLoadoutStateRef());
}

void UDivineBeastsAbilityBarViewModel::RefreshSlots(
    const FDivineBeastsAbilityLoadoutState& Snapshot)
{
    TArray<FGamePlatformUISlotState> NewSlots;
    NewSlots.Reserve(Snapshot.Slots.Num());
    TSet<FName> SeenSlots;
    const UDivineBeastsCharacterComponent* Identity = CharacterIdentity.Get();
    const bool bIdentityCurrent = Identity && Identity->IsCharacterReady() &&
        Identity->GetHeroDefinitionId() == Snapshot.HeroDefinitionId &&
        Identity->GetAvatarGeneration() == Snapshot.AvatarGeneration;
    if (!bIdentityCurrent)
    {
        // 实体已重生、重新选择英雄或新角色身份尚未确认时，立即删除旧槽位。
        // 不能只将上一个英雄的技能灰显：过期图标/技能说明属于另一角色作用域。
        if (!Slots.IsEmpty())
        {
            Slots.Reset();
            MarkStateChanged();
            SlotsChanged.Broadcast(Slots);
        }
        return;
    }
    for (const FDivineBeastsGrantedAbilitySlot& Grant : Snapshot.Slots)
    {
        if (Grant.AbilityId.IsNone() || Grant.SlotId.IsNone() ||
            Grant.AbilityLevel < 1 || SeenSlots.Contains(Grant.SlotId))
        {
            // 异常网络投影 fail-closed；不能以重复或未知槽位覆盖真实授权。
            continue;
        }
        SeenSlots.Add(Grant.SlotId);
        FGamePlatformUISlotState Slot;
        Slot.SlotId = Grant.SlotId;
        Slot.ContentId = Grant.AbilityId;
        Slot.Count = INDEX_NONE;
        Slot.OverlayProgress = 0.0f;
        Slot.bEnabled = false;
        Slot.bPending = !Snapshot.bReady || !bIdentityCurrent;
        if (Snapshot.bReady && bIdentityCurrent && Grant.InputTag.IsValid())
        {
            UGamePlatformAbilitySystemComponent* ASC = AbilitySystem.Get();
            const FGameplayAbilitySpec* Spec = nullptr;
            if (!ASC || !ASC->AbilityActorInfo.IsValid() ||
                !TryResolveGrantedSpec(*ASC, Grant.InputTag, Spec) ||
                !Spec || Spec->Level != Grant.AbilityLevel)
            {
                // OwnerOnly 自定义授权快照到达并不代表 GAS 原生 Spec 已完成复制。
                // 此时显示待确认而非默认“可使用”，由平台 Spec 复制完成事件触发再检查。
                Slot.bPending = true;
            }
            else
            {
                // 同一 InputTag 与等级还不足以证明这个技能属于该授权条目。
                // 项目配置型 Ability 必须额外用主资产编号验证，拒绝迟到或交叉的技能实例。
                const UDivineBeastsConfiguredGameplayAbility* Configured =
                    Cast<UDivineBeastsConfiguredGameplayAbility>(Spec->Ability);
                if (Configured &&
                    Configured->AbilityDefinitionId.PrimaryAssetName != Grant.AbilityId)
                {
                    Slot.bPending = true;
                    NewSlots.Add(MoveTemp(Slot));
                    continue;
                }

                // CanActivateAbility 只做原生 GAS 资格检查（成本、冷却、阻断/激活标签等），
                // 不执行 TryActivateAbility，更不会客户端授予技能或扣除资源。
                FGameplayTagContainer FailureTags;
                Slot.bEnabled = Spec->Ability->CanActivateAbility(
                    Spec->Handle, ASC->AbilityActorInfo.Get(),
                    nullptr, nullptr, &FailureTags);

                if (const FGameplayTagContainer* CooldownTags =
                        Spec->Ability->GetCooldownTags();
                    CooldownTags && !CooldownTags->IsEmpty())
                {
                    const FGameplayEffectQuery Query =
                        FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
                    const TArray<TPair<float, float>> RemainingAndDuration =
                        ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
                    for (const TPair<float, float>& Pair : RemainingAndDuration)
                    {
                        if (FMath::IsFinite(Pair.Key) && FMath::IsFinite(Pair.Value) &&
                            Pair.Value > KINDA_SMALL_NUMBER && Pair.Key > 0.0f)
                        {
                            Slot.OverlayProgress = FMath::Max(Slot.OverlayProgress,
                                FMath::Clamp(Pair.Key / Pair.Value, 0.0f, 1.0f));
                        }
                    }
                    if (Slot.OverlayProgress > 0.0f)
                    {
                        Slot.bEnabled = false;
                    }
                }
            }
        }
        if (LoadedProfile)
        {
            if (const FDivineBeastsAbilityUIEntry* UI = LoadedProfile->FindEntry(Grant.AbilityId))
            {
                Slot.Icon = UI->Icon;
            }
        }
        NewSlots.Add(MoveTemp(Slot));
    }

    bool bChanged = Slots.Num() != NewSlots.Num();
    if (!bChanged)
    {
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
        {
            const FGamePlatformUISlotState& A = Slots[Index];
            const FGamePlatformUISlotState& B = NewSlots[Index];
            if (A.SlotId != B.SlotId || A.ContentId != B.ContentId ||
                A.Icon != B.Icon || A.bEnabled != B.bEnabled ||
                A.bPending != B.bPending ||
                !FMath::IsNearlyEqual(A.OverlayProgress, B.OverlayProgress, 0.0001f))
            {
                bChanged = true;
                break;
            }
        }
    }
    if (bChanged)
    {
        Slots = MoveTemp(NewSlots);
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}
