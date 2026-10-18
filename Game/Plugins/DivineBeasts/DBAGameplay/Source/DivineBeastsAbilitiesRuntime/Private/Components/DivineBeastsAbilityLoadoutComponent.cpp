#include "Components/DivineBeastsAbilityLoadoutComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Abilities/DivineBeastsV6GameplayAbility.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"

#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformAbilitySetDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponent.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"

UDivineBeastsAbilityLoadoutComponent::UDivineBeastsAbilityLoadoutComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UDivineBeastsAbilityLoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    // 仅引擎新的BeginPlay可重新开放技能装配；不能由过期租约回调复活已退出的世界作用域。
    bEndingPlay = false;
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
    // 必须先失效可重入授权，再触发委托解绑/GAS取消/Data租约释放。
    // 否则效果移除的同步回调可在EndPlay栈中重新Grant并残留到已经退出的ASC。
    bEndingPlay = true;
    ++RequestSerial;
    if (UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get())
    {
        Character->OnReadinessChanged().Remove(ReadinessHandle);
    }
    ReadinessHandle.Reset();
    // Widget/观察者在销毁期不得接收新的Ready快照；回调可能同步触发EndPlay。
    Changed.Clear();
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        RevokeOwnedGrants();
    }
    CharacterIdentity.Reset();
    AbilitySystem.Reset();
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
    // 客户端可能在World退出期间收到最后一包复制，不允许已销毁的技能栏回弹。
    if (bEndingPlay) return;
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
    if (bEndingPlay || !IsValid(GetOwner()) || !GetOwner()->HasAuthority())
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
        const int32 ExpectedSerial = ++RequestSerial;
        RevokeOwnedGrants();
        // GAS撤销通知可能同步接管为新英雄/化身或发起新技能集请求。
        // 旧的NotReady回调仅可撤销当时的授权，不能在返回后清空后继Ready快照。
        if (ExpectedSerial != RequestSerial || !IsValid(GetOwner()) ||
            !GetOwner()->HasAuthority())
        {
            return;
        }
        PublishLoadout(NAME_None, 0, false, {});
    }
}

bool UDivineBeastsAbilityLoadoutComponent::AuthorityRefreshFromCharacter(FString& OutError)
{
    check(IsInGameThread());
    AActor* Owner = GetOwner();
    UDivineBeastsCharacterComponent* Character = CharacterIdentity.Get();
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (bEndingPlay || !IsValid(Owner) || !Owner->HasAuthority() ||
        !IsValid(Character) || !IsValid(ASC) || !Character->IsCharacterReady())
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
    // 开发覆盖只改变本次请求的集合身份，正式Hero资产不会持有NeverCook开发软引用。
    FString DevelopmentSetId;
    const bool bDevelopmentEnabled = DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess();
    if (bDevelopmentEnabled && GConfig)
    {
        GConfig->GetString(TEXT("DivineBeasts.Abilities.DevelopmentSets"),
            *Hero->DefinitionId.ToString(), DevelopmentSetId, GGameIni);
    }
    const FName RequestedSetId = DivineBeasts::DevelopmentAbilities::ResolveSetId(
        Hero->DefinitionId, Hero->DefaultAbilitySetId, bDevelopmentEnabled, DevelopmentSetId);
    if (RequestedSetId.IsNone())
    {
        const FName ExpectedHero = Hero->DefinitionId;
        const int32 ExpectedAvatar = Character->GetAvatarGeneration();
        const int32 ExpectedSerial = ++RequestSerial;
        RevokeOwnedGrants();
        // GAS撤销可同步触发角色/权限变更：旧空技能集请求不得清空后继角色的授权快照。
        if (ExpectedSerial == RequestSerial && CharacterIdentity.Get() == Character &&
            IsValid(Character) && Character->IsCharacterReady() &&
            Character->GetHeroDefinitionId() == ExpectedHero &&
            Character->GetAvatarGeneration() == ExpectedAvatar)
        {
            PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
        }
        OutError = TEXT("英雄尚未配置已批准的默认技能集合；禁止虚构技能。");
        return false;
    }

    if (LoadoutState.HeroDefinitionId == Hero->DefinitionId &&
        LoadoutState.AvatarGeneration == Character->GetAvatarGeneration() &&
        LoadoutState.bReady && PendingSetId == RequestedSetId)
    {
        OutError.Reset();
        return true;
    }
    // 同集合异步请求尚未完成时重复刷新是幂等的，不再重复 Acquire。
    if (DefinitionLease.IsValid() && PendingSetId == RequestedSetId &&
        LoadoutState.HeroDefinitionId == Hero->DefinitionId &&
        LoadoutState.AvatarGeneration == Character->GetAvatarGeneration())
    {
        OutError.Reset();
        return true;
    }

    const int32 Serial = ++RequestSerial;
    const FName ExpectedHero = Hero->DefinitionId;
    const int32 ExpectedAvatar = Character->GetAvatarGeneration();
    const auto IsRequestCurrent = [this, Serial, ExpectedHero, ExpectedAvatar, Character]()
    {
        const AActor* CurrentOwner = GetOwner();
        return !bEndingPlay && Serial == RequestSerial &&
            IsValid(CurrentOwner) && CurrentOwner->HasAuthority() &&
            CharacterIdentity.Get() == Character && IsValid(Character) &&
            Character->IsCharacterReady() && Character->GetHeroDefinitionId() == ExpectedHero &&
            Character->GetAvatarGeneration() == ExpectedAvatar;
    };
    RevokeOwnedGrants();
    if (!IsRequestCurrent())
    {
        OutError = TEXT("GAS旧授权撤销期间角色身份或请求代次已被接管。");
        return false;
    }
    PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
    if (!IsRequestCurrent())
    {
        OutError = TEXT("技能加载前的UI快照回调已经切换角色或技能请求。");
        return false;
    }
    IGamePlatformDataService* Data = FindDataService();
    if (!Data)
    {
        OutError = TEXT("当前游戏实例未提供平台定义数据服务，拒绝技能授权。");
        return false;
    }

    PendingSetId = RequestedSetId;
    const FPrimaryAssetId SetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(), PendingSetId);
    const TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> WeakSelf(this);
    FGamePlatformResult AcquireResult;
    // Data获取虽然合同要求延迟回调，仍以返回的局部句柄隔离可能同步触发的旧世界/换英雄通知。
    // 只有当前请求仍存活才能安装到组件；失败或旧请求只释放自身签发的租约。
    const FGamePlatformDataLease NewLease = Data->AcquireDefinition(
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
    if (!IsRequestCurrent() || PendingSetId != RequestedSetId)
    {
        if (NewLease.IsValid()) Data->ReleaseDefinition(NewLease);
        OutError = TEXT("技能数据获取期间当前英雄或化身已被后继请求接管。");
        return false;
    }
    if (!AcquireResult.IsSuccess() || !NewLease.IsValid())
    {
        PendingSetId = NAME_None;
        if (NewLease.IsValid()) Data->ReleaseDefinition(NewLease);
        OutError = TEXT("默认技能集合资源申请被平台数据服务拒绝。");
        return false;
    }
    DefinitionLease = NewLease;
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
    const auto IsRequestCurrent = [this, Character, ExpectedRequestSerial, ExpectedHero, ExpectedAvatar]()
    {
        const AActor* CurrentOwner = GetOwner();
        return !bEndingPlay && IsValid(CurrentOwner) && CurrentOwner->HasAuthority() &&
            ExpectedRequestSerial == RequestSerial && CharacterIdentity.Get() == Character &&
            IsValid(Character) && Character->IsCharacterReady() &&
            Character->GetHeroDefinitionId() == ExpectedHero &&
            Character->GetAvatarGeneration() == ExpectedAvatar;
    };
    if (!Data || !IsRequestCurrent() ||
        !DefinitionLease.IsValid() || !CompletedLease.IsValid() ||
        CompletedLease.ScopeId != DefinitionLease.ScopeId ||
        CompletedLease.LeaseId != DefinitionLease.LeaseId ||
        CompletedLease.Generation != DefinitionLease.Generation ||
        CompletedLease.IssuerProof != DefinitionLease.IssuerProof ||
        CompletedLease.DefinitionId != DefinitionLease.DefinitionId ||
        CompletedLease.ResourcePaths != DefinitionLease.ResourcePaths ||
        CompletedLease.Bundles != DefinitionLease.Bundles)
    {
        // 必须核对平台签发租约的完整身份；旧回调不得冒用新角色同名技能集。
        return;
    }
    if (Data->GetLeaseState(DefinitionLease) != EGamePlatformDataRequestState::Succeeded)
    {
        if (!IsRequestCurrent()) return;
        RevokeOwnedGrants();
        if (IsRequestCurrent()) PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
        return;
    }

    const UGamePlatformAbilitySetDefinition* Set = Cast<UGamePlatformAbilitySetDefinition>(
        Data->GetLoadedDefinition(DefinitionLease));
    FString Error;
    if (!Set || !ApplyAbilitySet(*Set, Error))
    {
        // 给Ability/GE授权的同步通知可触发后继请求；旧失败不应回滚后继成功。
        if (!IsRequestCurrent()) return;
        RevokeOwnedGrants();
        if (IsRequestCurrent()) PublishLoadout(ExpectedHero, ExpectedAvatar, false, {});
        return;
    }
    if (!IsRequestCurrent() || !DefinitionLease.IsValid()) return;

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
    if (!IsRequestCurrent() || !DefinitionLease.IsValid()) return;
    PublishLoadout(ExpectedHero, ExpectedAvatar, true, Slots);
}


bool UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible(
    const UGameplayEffect& Effect, float EffectLevel, FString& OutError)
{
    // 瞬时 GameplayEffect 没有可撤销句柄，不能用于需要原子回滚的 AbilitySet 启动授予。
    if (Effect.DurationPolicy == EGameplayEffectDurationType::Instant)
    {
        OutError = TEXT("默认技能集合启动效果必须可撤销，不能授予瞬时GameplayEffect。");
        return false;
    }

    // 周期效果与自定义执行计算可能已经触发无法撤销的治疗/伤害。
    const float PeriodSeconds = Effect.Period.GetValueAtLevel(EffectLevel);
    if (!FMath::IsFinite(PeriodSeconds) ||
        !FMath::IsNearlyZero(PeriodSeconds) ||
        !Effect.Executions.IsEmpty())
    {
        OutError = TEXT("默认技能集合启动效果不能包含周期计算或执行计算；请在技能激活阶段使用。");
        return false;
    }

    // 堆叠到其他来源已经持有的同类GE时，简单RemoveActiveGameplayEffect不能只撤销本次授予。
    if (Effect.GetStackingType() != EGameplayEffectStackingType::None)
    {
        OutError = TEXT("默认技能集合启动效果不允许与其他来源堆叠，必须保持独立可撤销句柄。");
        return false;
    }

    // UE5.8 的GameplayEffectComponent可能在应用/移除事件中创建额外效果、授权技能或执行自定义逻辑。
    // 首阶段没有经过逐类审计的白名单，不允许启动效果携带组件，避免移除主句柄后残留副作用。
    // UE5.8 的 GEComponents 为受保护成员，使用引擎公开的 FindComponent(BaseClass)
    // 判断是否存在任意派生组件，不越过封装读取受保护数组。
    if (Effect.FindComponent(UGameplayEffectComponent::StaticClass()) != nullptr)
    {
        OutError = TEXT("技能集启动效果附加GameplayEffectComponent尚未通过回滚安全审核，拒绝授予。");
        return false;
    }

    OutError.Reset();
    return true;
}

bool UDivineBeastsAbilityLoadoutComponent::ApplyAbilitySet(
    const UGamePlatformAbilitySetDefinition& Set, FString& OutError)
{
    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (bEndingPlay || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() ||
        !IsValid(ASC) || !Set.ValidateDefinition().IsSuccess())
    {
        OutError = TEXT("未通过可信服务器技能集合校验。");
        return false;
    }
    // GAS ApplyEffect/GiveAbility 都可能同步广播，后继角色可在当前栈内撤销旧租约。
    // 保留本次唯一请求身份，禁止外部回调使旧授权写进新角色的授权账本。
    const int32 GrantSerial = RequestSerial;
    const FName GrantSetId = PendingSetId;
    UDivineBeastsCharacterComponent* GrantCharacter = CharacterIdentity.Get();
    const FName GrantHero = IsValid(GrantCharacter) ? GrantCharacter->GetHeroDefinitionId() : NAME_None;
    const int32 GrantAvatar = IsValid(GrantCharacter) ? GrantCharacter->GetAvatarGeneration() : 0;
    const auto IsGrantCurrent = [this, ASC, GrantSerial, GrantSetId, GrantCharacter, GrantHero, GrantAvatar]()
    {
        const AActor* CurrentOwner = GetOwner();
        return !bEndingPlay && GrantSerial == RequestSerial &&
            !GrantSetId.IsNone() && PendingSetId == GrantSetId &&
            DefinitionLease.IsValid() && IsValid(CurrentOwner) && CurrentOwner->HasAuthority() &&
            AbilitySystem.Get() == ASC && IsValid(GrantCharacter) &&
            CharacterIdentity.Get() == GrantCharacter && GrantCharacter->IsCharacterReady() &&
            GrantCharacter->GetHeroDefinitionId() == GrantHero &&
            GrantCharacter->GetAvatarGeneration() == GrantAvatar;
    };
    if (!IsGrantCurrent())
    {
        OutError = TEXT("GAS技能授权开始时当前租约或角色代次已失效。");
        return false;
    }
    // DataService 的有效租约只证明签发身份，不能代替实际加载对象身份。
    // 如果资产注册或配置发生同ID串用，绝不授予另一个集合的数据。
    const FPrimaryAssetId ExpectedSetAssetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(), GrantSetId);
    if (Set.GetPrimaryAssetId() != ExpectedSetAssetId)
    {
        OutError = TEXT("实际加载的技能集合主资产身份与本次签发的集合ID不一致。");
        return false;
    }

    // Development客户端与Dedicated Server均可参加用户明确开启的验证；
    // 配置和启动参数缺一不可，Shipping/Test由编译期无条件关闭，即使开发目录被误Cook。
    if (Set.bDevelopmentOnly && !DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess())
    {
        OutError = TEXT("开发技能集合需要非Shipping/Test构建、显式开发配置与-DBADevelopmentSkills启动参数。");
        return false;
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
    TSet<const UClass*> NewAbilityClasses;
    for (const FGamePlatformAbilityGrant& Grant : Set.Abilities)
    {
        UClass* AbilityClass = Grant.AbilityClass.Get();
        if (!AbilityClass ||
            !AbilityClass->IsChildOf(UGamePlatformGameplayAbility::StaticClass()) ||
            AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            OutError = TEXT("技能类未预加载、反射类型非法或已经废弃，不允许进入GAS授权。");
            return false;
        }
        // 通用Definition已排除重复软路径；这里补上重定向、别名等不同软路径解析到同一UClass的情况。
        // 必须在授予第一个Spec前检查整个集合，禁止同类技能双重授权。
        if (NewAbilityClasses.Contains(AbilityClass))
        {
            OutError = TEXT("当前技能集合包含重复解析的真实技能类，拒绝重复授权。");
            return false;
        }
        NewAbilityClasses.Add(AbilityClass);
        // 同时拒绝与ASC原来其它来源已经持有的同一技能类冲突。
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
            // 项目能力在实例内记录每次Commit资格；NonInstanced可能跨激活复用类默认对象。
            // LocalOnly只执行客户端，不能成为专用服务器可信授权。
            if (Configured->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced ||
                Configured->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly)
            {
                OutError = TEXT("项目英雄技能禁止NonInstanced或LocalOnly网络执行策略。");
                return false;
            }
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
            // 八槽正式数据的机制和VFX尚未执行原生测试前，仅作设计资产展示。
            // 即使人为将草案GA加入某个AbilitySet，也绝不能给真实服务器授予空壳技能。
            if (Definition->bEightSlotDraftOnly)
            {
                OutError = TEXT("八槽技能属于未批准的设计数据资产，禁止GAS服务器授权。");
                return false;
            }
            // V6原生技能只接受被人工批准且通过完整服务器行为校验的定义。
            // 仅改DataAsset的草案布尔值或在编辑器里选择一个GA父类不足以构成授权。
            const UDivineBeastsV6GameplayAbility* V6Ability =
                Cast<UDivineBeastsV6GameplayAbility>(Configured);
            if (V6Ability && !Definition->bV6NativeApproved)
            {
                OutError = TEXT("V6技能没有原生执行资格，不授予当前角色。");
                return false;
            }
            if (Definition->bV6NativeApproved && !V6Ability)
            {
                OutError = TEXT("V6批准资产必须绑定正式V6执行器，不能借旧GA跳过服务器行为审核。");
                return false;
            }
            FDivineBeastsAbilityBalanceRow Balance;
            if (!Definition->TryGetLoadedBalance(Grant.AbilityLevel, Balance, OutError))
            {
                return false;
            }
            if (V6Ability && !V6Ability->ValidateNativeContract(*Definition, Balance, OutError))
            {
                // GAS成本及冷却稍后仍会依据真实GE进行二次交叉校验。
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
                // 数值行只声明单一Momentum负向修改。瞬时GE仍可能通过Execution、周期或
                // GameplayEffectComponent执行未声明的授权、伤害或状态修改，绝不可按Modifiers校验后放行。
                const float CostPeriod = CostEffect->Period.GetValueAtLevel(Grant.AbilityLevel);
                if (CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant ||
                    !FMath::IsFinite(CostPeriod) || !FMath::IsNearlyZero(CostPeriod) ||
                    !CostEffect->Executions.IsEmpty() ||
                    CostEffect->FindComponent(UGameplayEffectComponent::StaticClass()) != nullptr)
                {
                    OutError = TEXT("气势成本GE含非瞬时、周期、执行计算或未经审核的组件副作用，拒绝授予。");
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
                // 冷却效果负责授予持续标签，不允许借冷却GE额外执行伤害、治疗或资源改写。
                // 保留UE5.8的TargetTagsGameplayEffectComponent用于正常授予CooldownTags，
                // 不把冷却Tag组件误判为非法的启动GE组件。
                const float CooldownPeriod = CooldownEffect->Period.GetValueAtLevel(Grant.AbilityLevel);
                if (!CooldownEffect->Modifiers.IsEmpty() ||
                    !CooldownEffect->Executions.IsEmpty() ||
                    !FMath::IsFinite(CooldownPeriod) || !FMath::IsNearlyZero(CooldownPeriod))
                {
                    OutError = TEXT("冷却GE只能授予有限期标签，不得包含数值修改、周期或执行计算。");
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
        const UClass* EffectClass = Grant.EffectClass.Get();
        const UGameplayEffect* Effect = EffectClass
            ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
        if (!Effect)
        {
            OutError = TEXT("启动 GameplayEffect 尚未预加载或类型错误，拒绝部分授予。");
            return false;
        }
        if (!IsStartupEffectReversible(*Effect, Grant.Level, OutError))
        {
            // 全部效果须在首项GiveAbility或ApplyGameplayEffectToSelf之前预检，避免半事务残留。
            return false;
        }
    }

    // 依照既有平台授权合同：启动可回滚效果在先，技能Spec在后。
    // 单项失败由本组件调用方RevokeOwnedGrants按来源句柄撤销，绝不清空其它插件授予。
    for (const FGamePlatformEffectGrant& Grant : Set.Effects)
    {
        if (!IsGrantCurrent())
        {
            OutError = TEXT("GAS启动效果授权期间角色身份已被后继请求接管。");
            return false;
        }
        const UGameplayEffect* Effect =
            Grant.EffectClass.Get()->GetDefaultObject<UGameplayEffect>();
        const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
            Effect, Grant.Level, ASC->MakeEffectContext());
        if (!Handle.IsValid())
        {
            OutError = TEXT("可回滚启动效果应用失败，本次技能集合授予将撤销。");
            return false;
        }
        if (!IsGrantCurrent())
        {
            // 外部效果事件可能在返回真实Handle之前撤销旧账本，新效果尚未入账，必须就地撤销。
            if (IsValid(ASC)) ASC->RemoveActiveGameplayEffect(Handle);
            OutError = TEXT("GAS启动效果回调中技能请求已过期，撤销本次新建效果。");
            return false;
        }
        OwnedEffectHandles.Add(Handle);
    }

    for (const FGamePlatformAbilityGrant& Grant : Set.Abilities)
    {
        if (!IsGrantCurrent())
        {
            OutError = TEXT("GAS技能Spec授权期间角色身份已被后继请求接管。");
            return false;
        }
        FGameplayAbilitySpec Spec(Grant.AbilityClass.Get(), Grant.AbilityLevel);
        if (Grant.InputTag.IsValid())
        {
            Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
        }
        const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
        if (!Handle.IsValid())
        {
            OutError = TEXT("服务器 GAS 授予技能失败，本次启动效果及技能集合将撤销。");
            return false;
        }
        if (!IsGrantCurrent())
        {
            // 新Spec在GiveAbility同步通知之后才返回，外部账本尚未记账，按真实句柄单项撤销。
            if (IsValid(ASC))
            {
                ASC->CancelAbilityHandle(Handle);
                ASC->ClearAbility(Handle);
            }
            OutError = TEXT("GAS技能授权回调期间请求失效，撤销本次新建Spec。");
            return false;
        }
        OwnedAbilityHandles.Add(Handle);
    }
    OutError.Reset();
    return true;
}

void UDivineBeastsAbilityLoadoutComponent::RevokeOwnedGrants()
{
    // 先摘除当前组件拥有的真实句柄和租约，再调用GAS/平台Data外部入口。
    // CancelAbility/RemoveActiveGameplayEffect可能同步引发角色重绑和后继授权；
    // 旧调用只能撤销快照里的旧句柄，不能在返回后清空新请求的Owned数组与新租约。
    TArray<FGameplayAbilitySpecHandle> AbilitiesToRevoke = MoveTemp(OwnedAbilityHandles);
    TArray<FActiveGameplayEffectHandle> EffectsToRevoke = MoveTemp(OwnedEffectHandles);
    const FGamePlatformDataLease LeaseToRelease = DefinitionLease;
    OwnedAbilityHandles.Reset();
    OwnedEffectHandles.Reset();
    DefinitionLease = {};
    PendingSetId = NAME_None;

    UAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (IsValid(ASC) && IsValid(GetOwner()) && GetOwner()->HasAuthority())
    {
        for (const FGameplayAbilitySpecHandle Handle : AbilitiesToRevoke)
        {
            if (Handle.IsValid() && IsValid(ASC))
            {
                ASC->CancelAbilityHandle(Handle);
                if (IsValid(ASC)) ASC->ClearAbility(Handle);
            }
        }
        for (const FActiveGameplayEffectHandle& Handle : EffectsToRevoke)
        {
            if (Handle.IsValid() && IsValid(ASC))
            {
                ASC->RemoveActiveGameplayEffect(Handle);
            }
        }
    }
    if (LeaseToRelease.IsValid())
    {
        if (IGamePlatformDataService* Data = FindDataService())
        {
            Data->ReleaseDefinition(LeaseToRelease);
        }
    }
}

void UDivineBeastsAbilityLoadoutComponent::PublishLoadout(
    FName HeroDefinitionId, int32 AvatarGeneration, bool bReady,
    const TArray<FDivineBeastsGrantedAbilitySlot>& Slots)
{
    if (bEndingPlay) return; // 退出期绝不由旧委托发布新授权或空状态。
    LoadoutState.HeroDefinitionId = HeroDefinitionId;
    LoadoutState.AvatarGeneration = AvatarGeneration;
    if (LoadoutState.Revision < MAX_int64)
    {
        ++LoadoutState.Revision;
    }
    LoadoutState.bReady = bReady;
    LoadoutState.Slots = Slots;
    const int64 PublishedRevision = LoadoutState.Revision;
    Changed.Broadcast(LoadoutState);
    // 同步监听者可以换Pawn、启动下一次授权或结束世界；外层旧栈不能为后继状态再次发网络更新。
    if (bEndingPlay || LoadoutState.Revision != PublishedRevision) return;
    if (AActor* Owner = GetOwner(); IsValid(Owner) && Owner->HasAuthority())
    {
        Owner->ForceNetUpdate();
    }
}
