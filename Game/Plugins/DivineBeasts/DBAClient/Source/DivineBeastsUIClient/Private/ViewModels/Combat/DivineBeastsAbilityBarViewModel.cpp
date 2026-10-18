// 项目客户端技能栏：只读授权快照，Profile与图标由当前Loadout世界的Data租约持有。
// UObject/资源访问仅游戏线程；换英雄、解绑及销毁先撤代次再释放本VM的两份租约，不全局卸载。
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"

#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "GameFramework/PlayerController.h"

#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"

namespace
{
/** 完成快照可改变RequestState，其余完整签发身份必须等于当前持有租约。 */
bool IsSameUIResourceLease(const FGamePlatformDataLease& A, const FGamePlatformDataLease& B)
{
    return A.IsValid() && B.IsValid() && A.ScopeId == B.ScopeId && A.LeaseId == B.LeaseId &&
        A.Generation == B.Generation && A.IssuerProof == B.IssuerProof &&
        A.ResourcePaths == B.ResourcePaths && A.DefinitionId == B.DefinitionId && A.Bundles == B.Bundles;
}
}

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
    ResetProfileLease();
    if (!Slots.IsEmpty() || !SlotDetails.IsEmpty())
    {
        Slots.Reset();
        SlotDetails.Reset();
        MarkStateChanged();
        SlotsChanged.Broadcast(Slots);
    }
}

const FDivineBeastsGrantedAbilitySlot* UDivineBeastsAbilityBarViewModel::FindUniqueGrantedSlot(
    const FDivineBeastsAbilityLoadoutState& Snapshot, FName SlotId)
{
    if (SlotId.IsNone()) return nullptr;
    const FDivineBeastsGrantedAbilitySlot* Found = nullptr;
    for (const FDivineBeastsGrantedAbilitySlot& Candidate : Snapshot.Slots)
    {
        if (Candidate.SlotId != SlotId) continue;
        if (Found) return nullptr;
        Found = &Candidate;
    }
    return Found;
}

FGamePlatformResult UDivineBeastsAbilityBarViewModel::RequestActivateSlot(FName SlotId)
{
    check(IsInGameThread());
    UDivineBeastsAbilityLoadoutComponent* CurrentLoadout = Loadout.Get();
    UGamePlatformAbilitySystemComponent* ASC = AbilitySystem.Get();
    APawn* Pawn = CurrentLoadout ? Cast<APawn>(CurrentLoadout->GetOwner()) : nullptr;
    UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get();
    if (!IsValid(Pawn) || !Pawn->IsLocallyControlled() || !Pawn->GetController() ||
        Pawn->GetController()->IsMoveInputIgnored() || !IsValid(ASC) || !IsValid(Character))
        return FGamePlatformResult::Failure(TEXT("AbilityInputOwnerUnavailable"), TEXT("当前没有可操作的本地角色，无法提交技能输入。"));
    const FDivineBeastsAbilityLoadoutState Snapshot = CurrentLoadout->GetLoadoutState();
    if (!Snapshot.bReady || !Character->IsCharacterReady() ||
        Snapshot.HeroDefinitionId != Character->GetHeroDefinitionId() ||
        Snapshot.AvatarGeneration != Character->GetAvatarGeneration())
        return FGamePlatformResult::Failure(TEXT("AbilityLoadoutStale"), TEXT("当前技能授权或角色代次尚未确认。"));
    const FGamePlatformUISlotState* Display = Slots.FindByPredicate(
        [SlotId](const FGamePlatformUISlotState& Slot) { return Slot.SlotId == SlotId; });
    if (!Display || !Display->bEnabled || Display->bPending)
        return FGamePlatformResult::Failure(TEXT("AbilitySlotUnavailable"), TEXT("该技能尚未就绪、资源不足或正在冷却。"));
    const FDivineBeastsGrantedAbilitySlot* Grant = FindUniqueGrantedSlot(Snapshot, SlotId);
    if (!Grant) return FGamePlatformResult::Failure(TEXT("AbilitySlotAmbiguous"), TEXT("技能槽没有唯一授权，拒绝任选一个技能。"));
    const FGameplayAbilitySpec* Spec = nullptr;
    if (!Grant || !Grant->InputTag.IsValid() || Display->ContentId != Grant->AbilityId ||
        !TryResolveGrantedSpec(*ASC, Grant->InputTag, Spec) || !Spec || Spec->Level != Grant->AbilityLevel)
        return FGamePlatformResult::Failure(TEXT("AbilitySlotSpecUnconfirmed"), TEXT("该槽位没有当前已确认的主动技能实例。"));
    const auto* Configured = Cast<UDivineBeastsConfiguredGameplayAbility>(Spec->Ability);
    if (!Configured || Configured->AbilityDefinitionId.PrimaryAssetName != Grant->AbilityId)
        return FGamePlatformResult::Failure(TEXT("AbilitySlotIdentityMismatch"), TEXT("技能实例身份与服务器授权不一致。"));
    const FGamePlatformAbilityInputToken Token = ASC->GetInputToken();
    const FGameplayTag InputTag = Grant->InputTag;
    const FGamePlatformResult Result = ASC->AbilityInputPressed(InputTag, Token);
    if (!Result.IsSuccess()) return Result;
    ASC->ProcessAbilityInput();
    // 点击是一次输入脉冲；同步激活可能换Pawn，释放仍使用原令牌，禁止把旧点击留成按住状态。
    if (IsValid(ASC)) ASC->AbilityInputReleased(InputTag, Token);
    return Result;
}

void UDivineBeastsAbilityBarViewModel::BeginDestroy()
{
    UnbindFromLoadout();
    Super::BeginDestroy();
}

IGamePlatformDataService* UDivineBeastsAbilityBarViewModel::ResolveProfileDataService(UGameInstance* Instance) const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (DataServiceForTests) return DataServiceForTests;
#endif
    return IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
}

void UDivineBeastsAbilityBarViewModel::ResetProfileLease()
{
    check(IsInGameThread());
    ++LoadGeneration;
    const FGamePlatformDataLease OldProfile = ProfileLease;
    const FGamePlatformDataLease OldIcons = IconLease;
    UGameInstance* Instance = ProfileLeaseInstance.Get();
    // 先摘除本地所有权，释放通知或后续状态事件只能观察新代次，不能扣除后继租约。
    ProfileLease = {};
    IconLease = {};
    ProfileLeaseInstance.Reset();
    ProfilePath.Reset();
    LoadedProfile = nullptr;
    LoadedHeroId = NAME_None;
    bIconsReady = false;
    if (OldProfile.IsValid() || OldIcons.IsValid())
    {
        if (IGamePlatformDataService* Data = ResolveProfileDataService(Instance))
        {
            if (OldIcons.IsValid()) Data->ReleaseResources(OldIcons);
            if (OldProfile.IsValid()) Data->ReleaseResources(OldProfile);
        }
    }
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
    RefreshGameplayEligibilityBindings();

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
    if (AGamePlatformPlayerStateBase* State = LifecycleState.Get())
        State->OnLifecycleChanged.Remove(LifecycleHandle);
    if (UGamePlatformGameplayEligibilityComponent* Eligibility = GameplayEligibility.Get())
        Eligibility->OnEligibilityChanged().Remove(GameplayEligibilityHandle);
    LifecycleState.Reset(); GameplayEligibility.Reset();
    LifecycleHandle.Reset(); GameplayEligibilityHandle.Reset();
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
    RefreshGameplayEligibilityBindings();
    if (UDivineBeastsAbilityLoadoutComponent* Current = Loadout.Get())
    {
        RefreshSlots(Current->GetLoadoutStateRef());
    }
}

void UDivineBeastsAbilityBarViewModel::RefreshGameplayEligibilityBindings()
{
    check(IsInGameThread());
    const auto* Current = Loadout.Get();
    APawn* Pawn = Current ? Cast<APawn>(Current->GetOwner()) : nullptr;
    auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    auto* NewState = Controller ? Controller->GetPlayerState<AGamePlatformPlayerStateBase>() : nullptr;
    auto* NewEligibility = Pawn ? Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>() : nullptr;
    // 两个来源的复制顺序不固定：平台宿主查PlayerState，非平台宿主查中立资格组件。
    // 均需事件刷新；已有来源不重复绑定，换角色先撤旧来源再发布当前视图。
    if (LifecycleState.Get() != NewState)
    {
        if (auto* Previous = LifecycleState.Get()) Previous->OnLifecycleChanged.Remove(LifecycleHandle);
        LifecycleHandle.Reset(); LifecycleState = NewState;
        if (NewState) LifecycleHandle = NewState->OnLifecycleChanged.AddUObject(this, &ThisClass::RefreshCurrentLoadout);
    }
    if (GameplayEligibility.Get() != NewEligibility)
    {
        if (auto* Previous = GameplayEligibility.Get()) Previous->OnEligibilityChanged().Remove(GameplayEligibilityHandle);
        GameplayEligibilityHandle.Reset(); GameplayEligibility = NewEligibility;
        if (NewEligibility) GameplayEligibilityHandle = NewEligibility->OnEligibilityChanged().AddUObject(
            this, &ThisClass::HandleGameplayEligibilityChanged);
    }
}

void UDivineBeastsAbilityBarViewModel::HandleGameplayEligibilityChanged(
    const FGamePlatformGameplayEligibilitySnapshot& Snapshot)
{
    RefreshCurrentLoadout();
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
    check(IsInGameThread());
    RefreshGameplayEligibilityBindings();
    UDivineBeastsAbilityLoadoutComponent* Source = Loadout.Get();
    if (!Source) return;
    if (Snapshot.HeroDefinitionId != LoadedHeroId)
    {
        ResetProfileLease();
        LoadedHeroId = Snapshot.HeroDefinitionId;
        if (!LoadedHeroId.IsNone())
        {
            const FPrimaryAssetId ProfileId(
                FPrimaryAssetType(TEXT("DivineBeastsAbilityUIProfile")), LoadedHeroId);
            // AssetManager只读查询已登记主资产路径；保留原身份，不把普通Profile冒充平台Definition。
            if (UAssetManager* Assets = UAssetManager::GetIfInitialized())
                ProfilePath = Assets->GetPrimaryAssetPath(ProfileId);
#if WITH_DEV_AUTOMATION_TESTS
            if (ProfilePathForTests) ProfilePath = ProfilePathForTests(LoadedHeroId);
#endif
            UWorld* World = Source->GetWorld();
            UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
            IGamePlatformDataService* Data = ResolveProfileDataService(Instance);
            if (Data && IsValid(Instance) && World && !World->bIsTearingDown && ProfilePath.IsValid())
            {
                ProfileLeaseInstance = Instance;
                const uint64 Serial = LoadGeneration;
                const FName Hero = LoadedHeroId;
                const TWeakObjectPtr<UDivineBeastsAbilityBarViewModel> WeakThis(this);
                const TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> WeakSource(Source);
                FGamePlatformResult Accepted;
                // Loadout拥有真实World/GI，VM由Widget持有可能没有GetWorld；分别校验弱来源和弱VM。
                const FGamePlatformDataLease Lease = Data->AcquireResources(
                    { ProfilePath }, EGamePlatformDataLifetime::World, Source,
                    [WeakThis, WeakSource, Serial, Hero](const FGamePlatformDataLease& Completed, const FGamePlatformResult& Result)
                    {
                        if (auto* Self = WeakThis.Get(); Self && WeakSource.IsValid() && Self->Loadout == WeakSource)
                            Self->HandleProfileLoaded(Serial, Hero, Completed, Result);
                    }, Accepted);
                if (Serial == LoadGeneration && Loadout.Get() == Source && Accepted.IsSuccess() && Lease.IsValid())
                    ProfileLease = Lease;
                else if (Lease.IsValid())
                    Data->ReleaseResources(Lease);
            }
        }
    }
    RefreshSlots(Snapshot);
}

void UDivineBeastsAbilityBarViewModel::HandleProfileLoaded(
    uint64 ExpectedLoadGeneration, FName ExpectedHero,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (ExpectedLoadGeneration != LoadGeneration || ExpectedHero != LoadedHeroId ||
        !Loadout.IsValid() || !IsSameUIResourceLease(ProfileLease, Lease))
        return; // 旧请求已由换英雄/解绑路径释放；不能触碰后继的租约。
    IGamePlatformDataService* Data = ResolveProfileDataService(ProfileLeaseInstance.Get());
    UDivineBeastsAbilityUIProfile* Profile = Result.IsSuccess() && Data &&
        Data->GetLeaseState(ProfileLease) == EGamePlatformDataRequestState::Succeeded
        ? Cast<UDivineBeastsAbilityUIProfile>(ProfilePath.ResolveObject()) : nullptr;
    FString Error;
    if (!Profile || Profile->HeroDefinitionId != ExpectedHero || !Profile->ValidateProfile(Error))
    {
        const FGamePlatformDataLease FailedLease = ProfileLease;
        ProfileLease = {};
        if (Data) Data->ReleaseResources(FailedLease);
        RefreshCurrentLoadout();
        return;
    }

    LoadedProfile = Profile;
    TArray<FSoftObjectPath> IconPaths;
    for (const FDivineBeastsAbilityUIEntry& Entry : Profile->Entries)
        if (!Entry.Icon.IsNull()) IconPaths.AddUnique(Entry.Icon.ToSoftObjectPath());
    if (!IconPaths.IsEmpty())
    {
        FGamePlatformResult Accepted;
        const TWeakObjectPtr<UDivineBeastsAbilityBarViewModel> WeakThis(this);
        const TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> WeakSource(Loadout);
        const FGamePlatformDataLease NewIcons = Data->AcquireResources(
            IconPaths, EGamePlatformDataLifetime::World, Loadout.Get(),
            [WeakThis, WeakSource, ExpectedLoadGeneration, ExpectedHero](
                const FGamePlatformDataLease& Completed, const FGamePlatformResult& IconResult)
            {
                if (auto* Self = WeakThis.Get(); Self && WeakSource.IsValid() && Self->Loadout == WeakSource)
                    Self->HandleIconsLoaded(ExpectedLoadGeneration, ExpectedHero, Completed, IconResult);
            }, Accepted);
        if (ExpectedLoadGeneration != LoadGeneration || !Loadout.IsValid())
        {
            if (NewIcons.IsValid()) Data->ReleaseResources(NewIcons);
            return;
        }
        if (Accepted.IsSuccess() && NewIcons.IsValid()) IconLease = NewIcons;
        else if (NewIcons.IsValid()) Data->ReleaseResources(NewIcons);
    }
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::HandleIconsLoaded(
    uint64 ExpectedLoadGeneration, FName ExpectedHero,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (ExpectedLoadGeneration != LoadGeneration || ExpectedHero != LoadedHeroId ||
        !Loadout.IsValid() || !IsSameUIResourceLease(IconLease, Lease))
        return;
    IGamePlatformDataService* Data = ResolveProfileDataService(ProfileLeaseInstance.Get());
    bIconsReady = Result.IsSuccess() && Data &&
        Data->GetLeaseState(IconLease) == EGamePlatformDataRequestState::Succeeded;
    if (!bIconsReady)
    {
        const FGamePlatformDataLease FailedLease = IconLease;
        IconLease = {};
        if (Data) Data->ReleaseResources(FailedLease);
    }
    // 图标失败保留已验证的本地化文本，图标为空；资源失败不会改写GAS授权或就绪资格。
    RefreshCurrentLoadout();
}

void UDivineBeastsAbilityBarViewModel::RefreshSlots(
    const FDivineBeastsAbilityLoadoutState& Snapshot)
{
    TArray<FGamePlatformUISlotState> NewSlots;
    NewSlots.Reserve(Snapshot.Slots.Num());
    TArray<FDivineBeastsAbilitySlotDetails> NewDetails;
    NewDetails.Reserve(Snapshot.Slots.Num());
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
            Grant.AbilityLevel < 1 || FindUniqueGrantedSlot(Snapshot, Grant.SlotId) != &Grant)
        {
            // 先核对完整快照；重复槽的每个候选均拒绝，不能留下数组中的第一项。
            continue;
        }
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
                // 按能力语义判断沉默，数字1也可绑定法术，不能用输入槽冒充普通攻击。
                const bool bPrimaryAttack = !Spec->Ability->GetAssetTags().HasTag(
                    FGameplayTag::RequestGameplayTag(TEXT("Ability.Spell")));
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
                                // 匹配SlateRHIRenderer的RealTime=PlatformTime-GStartTime；
                                // 只在权威效果事件更新，渲染材质自行插值，不增加UI Tick。
                                Detail.CooldownEndsAtAppSeconds = static_cast<float>(FPlatformTime::Seconds() - GStartTime) + Pair.Key;
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
                // 这里仅读取当前本地拥有者GAS的已复制成本事实；按钮提示不是服务器授权或第二套扣费。
                if (!Slot.bEnabled && Detail.DisabledReason.IsEmpty() &&
                    Configured->GetCostGameplayEffect() &&
                    !Spec->Ability->CheckCost(Spec->Handle, ASC->AbilityActorInfo.Get()))
                {
                    Detail.DisabledReason = FText::FromString(TEXT("气势不足"));
                }
            }
        }
        if (LoadedProfile)
        {
            if (const FDivineBeastsAbilityUIEntry* UI = LoadedProfile->FindEntry(Grant.AbilityId))
            {
                if (bIconsReady && UI->Icon.IsValid()) Slot.Icon = UI->Icon;
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
                !FMath::IsNearlyEqual(A.CooldownTotalSeconds, B.CooldownTotalSeconds, 0.01f) ||
                // 同时长冷却被刷新时，剩余秒数与遮罩比例可能看起来相同，
                // 但新真实GE对应的材质截止时间已变；不发布会让遮罩沿旧时间继续走。
                !FMath::IsNearlyEqual(A.CooldownEndsAtAppSeconds, B.CooldownEndsAtAppSeconds, 0.01f))
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
