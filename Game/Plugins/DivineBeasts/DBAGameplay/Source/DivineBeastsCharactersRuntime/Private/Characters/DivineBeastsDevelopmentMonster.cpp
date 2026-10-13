// 项目开发NPC生命周期：游戏线程只读GAS/CombatState并复制显示快照；自身监听和操作身份随Actor结束。
#include "Characters/DivineBeastsDevelopmentMonster.h"

#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "Types/GamePlatformCombatEvent.h"
#include "UObject/StrongObjectPtr.h"

ADivineBeastsDevelopmentMonster::ADivineBeastsDevelopmentMonster()
{
    bReplicates = true;
    SetReplicateMovement(true);
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::Disabled;
    PrimaryActorTick.bCanEverTick = false;
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    // 无玩家Owner的NPC只复制必要GE Tag/Cue，生命属性和本Actor快照仍由各自原生复制负责。
    GetGamePlatformAbilitySystemComponent()->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

bool ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!IsInGameThread()) return false;
    bool bAllowed = false;
    if (GConfig)
        GConfig->GetBool(TEXT("DivineBeasts.Development"), TEXT("bAllowDevelopmentMonsters"), bAllowed, GGameIni);
    return bAllowed && FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentMonsters"));
#else
    return false;
#endif
}

FName ADivineBeastsDevelopmentMonster::GetDevelopmentMonsterId() const
{ return FName(TEXT("DevelopmentMonster.AncientStoneSpirit")); }

FText ADivineBeastsDevelopmentMonster::GetMonsterDisplayName() const
{ return NSLOCTEXT("DivineBeastsDevelopmentMonster", "AncientStoneSpirit", "远古石灵"); }

bool ADivineBeastsDevelopmentMonster::CanSourceCombat() const { return false; }

bool ADivineBeastsDevelopmentMonster::CanReceiveCombat() const
{
    const auto* ASC = GetGamePlatformAbilitySystemComponent();
    const auto* CombatState = GetGamePlatformCombatComponent();
    const UWorld* World = GetWorld();
    return IsDevelopmentEnabledForCurrentProcess() && !bMonsterClosing && !bLifeTransitionInProgress &&
        !IsActorBeingDestroyed() && HasActorBegunPlay() && World && !World->bIsTearingDown &&
        (HasAuthority() ? bLifeInitialized : LifeState.AvatarGeneration > 0) &&
        IsValid(ASC) && ASC->GetOwnerActor() == this && ASC->GetAvatarActor() == this &&
        IsValid(CombatState) && !CombatState->IsCombatDead() && CombatState->GetCombatHealth() > 0.0f &&
        LifeState.AvatarGeneration > 0 && LifeState.AvatarGeneration == CombatState->GetCombatAvatarGeneration();
}

int32 ADivineBeastsDevelopmentMonster::GetCombatAvatarGeneration() const
{
    const auto* CombatState = GetGamePlatformCombatComponent();
    return CombatState ? CombatState->GetCombatAvatarGeneration() : 0;
}

void ADivineBeastsDevelopmentMonster::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->DisableMovement();
    auto* ASC = GetGamePlatformAbilitySystemComponent();
    auto* CombatState = GetGamePlatformCombatComponent();
    if (IsValid(ASC) && IsValid(CombatState))
    {
        HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
            UGamePlatformCombatAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleVitalAttributeChanged);
        MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
            UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleVitalAttributeChanged);
        CombatState->OnCombatEvent.AddDynamic(this, &ThisClass::HandleMonsterCombatEvent);
    }
    // 不自动初始化或生成Actor；开发关闭时原生类型保持无有效生命，Server装配不创建它。
}

bool ADivineBeastsDevelopmentMonster::AuthorityInitializeDevelopmentMonster()
{ return TransitionToNewLife(false, 0); }

bool ADivineBeastsDevelopmentMonster::AuthorityRespawnDevelopmentMonster(int32 ExpectedAvatarGeneration)
{ return TransitionToNewLife(true, ExpectedAvatarGeneration); }

bool ADivineBeastsDevelopmentMonster::TransitionToNewLife(bool bRespawn, int32 ExpectedAvatarGeneration)
{
    UWorld* World = GetWorld();
    auto* ASC = GetGamePlatformAbilitySystemComponent();
    auto* CombatState = GetGamePlatformCombatComponent();
    if (!IsDevelopmentEnabledForCurrentProcess() || !HasAuthority() || bMonsterClosing ||
        bLifeTransitionInProgress || LifeNotificationDepth > 0 || IsActorBeingDestroyed() ||
        !HasActorBegunPlay() || GetController() || !IsValid(World) || World->bIsTearingDown ||
        !IsValid(ASC) || !IsValid(CombatState) || !CombatState->HasBegunPlay() || !CombatState->GetCombatAttributeSet()) return false;
    const int32 PreviousGeneration = CombatState->GetCombatAvatarGeneration();
    if (PreviousGeneration <= 0 || PreviousGeneration == MAX_int32 ||
        (bRespawn ? (!bLifeInitialized || !CombatState->IsCombatDead() || ExpectedAvatarGeneration <= 0 ||
            ExpectedAvatarGeneration != PreviousGeneration) : bLifeInitialized)) return false;

    // 强引用保证同步通知销毁时旧栈仍可安全核查；操作身份与世界生命周期决定是否允许继续写。
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepActor(this);
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const TStrongObjectPtr<UGamePlatformAbilitySystemComponent> KeepASC(ASC);
    const TStrongObjectPtr<UGamePlatformCombatComponent> KeepCombat(CombatState);
    const FGuid Operation = FGuid::NewGuid();
    LifeTransitionId = Operation;
    bLifeTransitionInProgress = true;
    // 写权限撤销必须阻断后续GAS操作，但不能阻断本操作释放自己的锁。
    // EndPlay或后继操作先改变身份时不碰其状态；不回滚真实GAS属性，不清其他操作的锁。
    ON_SCOPE_EXIT
    {
        if (LifeTransitionId == Operation) bLifeTransitionInProgress = false;
    };
    const auto IsCurrent = [this, Operation, World, ASC, CombatState]
    {
        return IsValid(this) && !bMonsterClosing && !IsActorBeingDestroyed() && HasAuthority() &&
            LifeTransitionId == Operation && GetWorld() == World && IsValid(World) && !World->bIsTearingDown &&
            GetGamePlatformAbilitySystemComponent() == ASC && IsValid(ASC) &&
            GetGamePlatformCombatComponent() == CombatState && IsValid(CombatState) &&
            IsDevelopmentEnabledForCurrentProcess();
    };
    const bool bBound = ASC->BindAbilityActorInfo(this, this);
    if (!IsCurrent()) return false;
    if (!bBound || ASC->GetAvatarActor() != this || ASC->GetOwnerActor() != this)
        return false;
    ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute(), 300.0f);
    if (!IsCurrent()) return false;
    const bool bReset = CombatState->ResetForNewAvatar(PreviousGeneration + 1, 1.0f);
    if (!IsCurrent())
    {
        // Reset的真实Health通知可撤销开发许可，但平台仍合法提交新代次活体；此时旧死投影已不可重试。
        // 不回滚GAS或假装项目重生成功：只在本Actor/World/操作及恰好本次提交代次仍归自己时权威退役。
        // EndPlay、换世界、组件替换或后继Reset已接管时绝不销毁；尚未进入Reset的初始化撤权仍走原可重试路径。
        if (bReset && IsValid(this) && HasAuthority() && !bMonsterClosing && !IsActorBeingDestroyed() &&
            LifeTransitionId == Operation && GetWorld() == World && IsValid(World) && !World->bIsTearingDown &&
            GetGamePlatformAbilitySystemComponent() == ASC && IsValid(ASC) &&
            ASC->GetOwnerActor() == this && ASC->GetAvatarActor() == this &&
            GetGamePlatformCombatComponent() == CombatState && IsValid(CombatState) && CombatState->GetOwner() == this &&
            CombatState->GetCombatAvatarGeneration() == PreviousGeneration + 1 && !CombatState->IsCombatDead() &&
            CombatState->GetCombatHealth() > 0.0f)
        {
            // Destroy同步路由EndPlay并失效操作身份；此后只返回失败，不再发布生命或操作GAS/胶囊。
            if (!Destroy()) UE_LOG(LogTemp, Error, TEXT("开发石灵已提交生命但项目操作失效，权威退役失败"));
        }
        return false;
    }
    if (!bReset || CombatState->GetCombatAvatarGeneration() != PreviousGeneration + 1 || CombatState->IsCombatDead() ||
        !FMath::IsNearlyEqual(CombatState->GetCombatHealth(), 300.0f) ||
        !FMath::IsNearlyEqual(CombatState->GetCombatMaxHealth(), 300.0f)) return false;

    bLifeInitialized = true;
    bLifeTransitionInProgress = false;
    // Reset提交后才开放碰撞和发布完整新生命；中间GAS变化不泄露成成功的重生快照。
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (!IsCurrent()) return false;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    if (!IsCurrent()) return false;
    PublishCombatLifeState();
    // 通知可能关闭Actor或建立后继状态；不尾写快照，不把后继清理结果覆盖成旧调用成功。
    return IsCurrent() && bLifeInitialized && CombatState->GetCombatAvatarGeneration() == PreviousGeneration + 1;
}

void ADivineBeastsDevelopmentMonster::HandleVitalAttributeChanged(const FOnAttributeChangeData& Change)
{
    (void)Change;
    if (HasAuthority()) PublishCombatLifeState();
    // 客户端显示只消费原子LifeState复制；不能把先到的GAS Health与另一代次拼成新快照。
}

void ADivineBeastsDevelopmentMonster::HandleMonsterCombatEvent(const FGamePlatformCombatEvent& Event)
{
    const auto* CombatState = GetGamePlatformCombatComponent();
    if (!HasAuthority() || bMonsterClosing || !IsValid(CombatState) || Event.TargetActor != this ||
        Event.TargetAvatarGeneration != CombatState->GetCombatAvatarGeneration()) return;
    if (Event.EventType == EGamePlatformCombatEventType::Death && CombatState->IsCombatDead())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (bMonsterClosing || IsActorBeingDestroyed()) return;
    }
    PublishCombatLifeState();
}

void ADivineBeastsDevelopmentMonster::PublishCombatLifeState()
{
    const auto* CombatState = GetGamePlatformCombatComponent();
    if (!HasAuthority() || bMonsterClosing || IsActorBeingDestroyed() || !bLifeInitialized ||
        bLifeTransitionInProgress || !IsValid(CombatState)) return;
    FDivineBeastsDevelopmentMonsterLifeState Next;
    Next.Health = CombatState->GetCombatHealth();
    Next.MaxHealth = CombatState->GetCombatMaxHealth();
    Next.bDead = CombatState->IsCombatDead();
    Next.AvatarGeneration = CombatState->GetCombatAvatarGeneration();
    // GAS生命先归零，CombatState随后提交死亡；该中间值不能让观察者尝试重生。
    if (!Next.bDead && Next.Health <= 0.0f) return;
    if (Next.Health == LifeState.Health && Next.MaxHealth == LifeState.MaxHealth &&
        Next.bDead == LifeState.bDead && Next.AvatarGeneration == LifeState.AvatarGeneration) return;
    LifeState = Next;
    ForceNetUpdate();
    BroadcastLifeStateValue(Next);
}

void ADivineBeastsDevelopmentMonster::BroadcastLifeStateValue(const FDivineBeastsDevelopmentMonsterLifeState& State)
{
    // 必须再次值拷贝：OnRep传入的成员和同步后继伤害都不能改变本次const&通知的内容。
    const FDivineBeastsDevelopmentMonsterLifeState Published = State;
    ++LifeNotificationDepth;
    LifeStateChanged.Broadcast(Published);
    --LifeNotificationDepth;
}

void ADivineBeastsDevelopmentMonster::OnRep_LifeState()
{
    UWorld* World = GetWorld();
    if (bMonsterClosing || IsActorBeingDestroyed() || LifeState.AvatarGeneration <= 0 ||
        !IsValid(World) || World->bIsTearingDown) return;
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepActor(this);
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const FDivineBeastsDevelopmentMonsterLifeState Received = LifeState;
    const auto IsCurrent = [this, World, Received]
    {
        return !bMonsterClosing && !IsActorBeingDestroyed() && GetWorld() == World &&
            IsValid(World) && !World->bIsTearingDown && LifeState.AvatarGeneration == Received.AvatarGeneration &&
            LifeState.Health == Received.Health && LifeState.MaxHealth == Received.MaxHealth && LifeState.bDead == Received.bDead;
    };
    // CollisionEnabled不是复制生命事实本身；接收端必须显式投影死亡/重生，不能依赖服务端过滤射线。
    // 这里只读Received，不写客户端GAS/Combat/生命代次，亦不让OnRep成为新的权威重生入口。
    if (!HasAuthority())
    {
        auto* Capsule = GetCapsuleComponent();
        if (!IsValid(Capsule)) return;
        Capsule->SetCollisionEnabled(Received.bDead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        if (!IsCurrent()) return;
        if (!Received.bDead)
        {
            Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            if (!IsCurrent()) return;
        }
    }
    // 胶囊更新可同步退出或触发后继复制回调；外层不得再发布已经被新事实取代的旧通知。
    if (IsCurrent()) BroadcastLifeStateValue(Received);
}

void ADivineBeastsDevelopmentMonster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADivineBeastsDevelopmentMonster, LifeState);
}

void ADivineBeastsDevelopmentMonster::EndPlay(const EEndPlayReason::Type Reason)
{
    bMonsterClosing = true;
    LifeTransitionId.Invalidate();
    bLifeInitialized = false;
    auto* ASC = GetGamePlatformAbilitySystemComponent();
    if (IsValid(ASC))
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
    }
    HealthChangedHandle.Reset();
    MaxHealthChangedHandle.Reset();
    if (auto* CombatState = GetGamePlatformCombatComponent())
        CombatState->OnCombatEvent.RemoveDynamic(this, &ThisClass::HandleMonsterCombatEvent);
    LifeStateChanged.Clear();
    Super::EndPlay(Reason);
}
