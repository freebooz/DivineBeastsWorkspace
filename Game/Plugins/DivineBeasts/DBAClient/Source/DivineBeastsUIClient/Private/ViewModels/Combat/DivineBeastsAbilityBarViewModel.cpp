#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"

#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"

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
    if (!Slots.IsEmpty() || !SlotDetails.IsEmpty())
    {
        Slots.Reset();
        SlotDetails.Reset();
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
    TArray<FDivineBeastsAbilitySlotDetails> NewDetails;
    NewDetails.Reserve(Snapshot.Slots.Num());
    TSet<FName> SeenSlots;
    const UDivineBeastsCharacterComponent* Identity = CharacterIdentity.Get();
    const bool bIdentityCurrent = Identity && Identity->IsCharacterReady() &&
        Identity->GetHeroDefinitionId() == Snapshot.HeroDefinitionId &&
        Identity->GetAvatarGeneration() == Snapshot.AvatarGeneration;
    if (!bIdentityCurrent)
    {
        // 实体已重生、重新选择英雄或新角色身份尚未确认时，立即删除旧槽位。
        // 不能只将上一个英雄的技能灰显：过期图标/技能说明属于另一角色作用域。
        if (!Slots.IsEmpty() || !SlotDetails.IsEmpty())
        {
            Slots.Reset();
            SlotDetails.Reset();
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
        FDivineBeastsAbilitySlotDetails Detail;
        Detail.SlotId = Grant.SlotId;
        Detail.AbilityId = Grant.AbilityId;
        Detail.AbilityLevel = Grant.AbilityLevel;
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
                if (!Configured ||
                    Configured->AbilityDefinitionId.PrimaryAssetName != Grant.AbilityId)
                {
                    Slot.bPending = true;
                    Detail.DisabledReason = FText::FromString(TEXT("技能编号尚未与授权实例一致"));
                    NewDetails.Add(MoveTemp(Detail));
                    NewSlots.Add(MoveTemp(Slot));
                    continue;
                }

                // CanActivateAbility 只做原生 GAS 资格检查（成本、冷却、阻断/激活标签等），
                // 不执行 TryActivateAbility，更不会客户端授予技能或扣除资源。
                FGameplayTagContainer FailureTags;
                Slot.bEnabled = Spec->Ability->CanActivateAbility(
                    Spec->Handle, ASC->AbilityActorInfo.Get(),
                    nullptr, nullptr, &FailureTags);

                // 平台战斗标签是权威 GameplayEffect 的复制事实：死亡/眩晕禁用全部动作，
                // 沉默只禁用非普通攻击技能。UI只改变按钮样式，不能反向决定服务器资格。
                const bool bDead = ASC->HasMatchingGameplayTag(
                    GamePlatformCombatTags::State_Dead);
                const bool bStunned = ASC->HasMatchingGameplayTag(
                    GamePlatformCombatTags::Control_Stun);
                const bool bSilenced = ASC->HasMatchingGameplayTag(
                    GamePlatformCombatTags::Control_Silence);
                const bool bPrimaryAttack =
                    Grant.InputTag.ToString().EndsWith(TEXT(".Primary"));
                if (bDead || bStunned || (bSilenced && !bPrimaryAttack))
                {
                    Slot.bEnabled = false;
                }
                if (bDead)
                {
                    Detail.DisabledReason = FText::FromString(TEXT("角色已阵亡"));
                }
                else if (bStunned)
                {
                    Detail.DisabledReason = FText::FromString(TEXT("眩晕中无法使用技能"));
                }
                else if (bSilenced && !bPrimaryAttack)
                {
                    Detail.DisabledReason = FText::FromString(TEXT("沉默中无法施法"));
                }

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
                            if (Pair.Key > Detail.CooldownRemainingSeconds)
                            {
                                Detail.CooldownRemainingSeconds = Pair.Key;
                                Detail.CooldownTotalSeconds = Pair.Value;
                            }
                        }
                    }
                    if (Slot.OverlayProgress > 0.0f)
                    {
                        Slot.bEnabled = false;
                    }
                    if (Slot.OverlayProgress > 0.0f && Detail.DisabledReason.IsEmpty())
                    {
                        // 优先显示死亡、眩晕或沉默等更重要的封禁原因。
                        Detail.DisabledReason = FText::FromString(TEXT("技能冷却中"));
                    }
                }
            }
        }
        if (LoadedProfile)
        {
            if (const FDivineBeastsAbilityUIEntry* UI = LoadedProfile->FindEntry(Grant.AbilityId))
            {
                Slot.Icon = UI->Icon;
                Detail.DisplayName = UI->DisplayName;
                Detail.Description = UI->Description;
            }
        }
        if (Detail.DisplayName.IsEmpty())
        {
            // 未取得已批准的UI Profile时，不把内部AbilityId展示成游戏技能名称。
            Detail.DisplayName = FText::FromString(TEXT("技能资料尚未加载"));
        }
        if (Slot.bPending && Detail.DisabledReason.IsEmpty())
        {
            Detail.DisabledReason = FText::FromString(TEXT("技能授权或角色状态待确认"));
        }
        else if (!Slot.bEnabled && Detail.DisabledReason.IsEmpty())
        {
            Detail.DisabledReason = FText::FromString(TEXT("当前条件不允许使用技能"));
        }
        NewDetails.Add(MoveTemp(Detail));
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
    bool bDetailsChanged = SlotDetails.Num() != NewDetails.Num();
    if (!bDetailsChanged)
    {
        for (int32 Index = 0; Index < NewDetails.Num(); ++Index)
        {
            const FDivineBeastsAbilitySlotDetails& A = SlotDetails[Index];
            const FDivineBeastsAbilitySlotDetails& B = NewDetails[Index];
            if (A.SlotId != B.SlotId || A.AbilityId != B.AbilityId ||
                A.AbilityLevel != B.AbilityLevel ||
                !A.DisplayName.EqualTo(B.DisplayName) ||
                !A.Description.EqualTo(B.Description) ||
                !A.DisabledReason.EqualTo(B.DisabledReason) ||
                !FMath::IsNearlyEqual(A.CooldownRemainingSeconds, B.CooldownRemainingSeconds, 0.01f) ||
                !FMath::IsNearlyEqual(A.CooldownTotalSeconds, B.CooldownTotalSeconds, 0.01f))
            {
                bDetailsChanged = true;
                break;
            }
        }
    }

    if (bChanged || bDetailsChanged)
    {
        Slots = MoveTemp(NewSlots);
        SlotDetails = MoveTemp(NewDetails);
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}
