// 平台服务器AI控制器：当前Pawn/World拥有感知、候选和技能Gate；游戏线程决策，退出解绑/取消，不维护项目或全局玩家事实。
#include "Controllers/GamePlatformAIController.h"

#include "Abilities/AIAbilityActivationPolicy.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Blackboard/GamePlatformAIBlackboardKeys.h"
#include "BrainComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Interfaces/GamePlatformAITargetEligibilityProvider.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Perception/AIWorldPolicy.h"
#include "Perception/AICandidateRetention.h"
#include "Navigation/PathFollowingComponent.h"
#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Tags/GamePlatformAbilitySystemTags.h"
#include "Tags/GamePlatformAITags.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Targeting/GamePlatformAITargetSelectionRules.h"
#include "TimerManager.h"

namespace
{
/** 服务器AI的只读Gate：弱Controller和两种代次保护，事实从实际Controller读取，无固定成功或项目依赖。 */
class FGamePlatformAIActivationGate final : public IGamePlatformAbilityActivationGate
{
public:
    FGamePlatformAIActivationGate(AGamePlatformAIController& Owner, int32 ResourceGeneration, int32 AIInstanceGeneration)
        : Controller(&Owner), ExpectedResourceGeneration(ResourceGeneration), ExpectedAIInstanceGeneration(AIInstanceGeneration) {}
    FGamePlatformResult Evaluate(const UGamePlatformAbilitySystemComponent& Component) const override
    {
        const auto* Owner = Controller.Get();
        return Owner ? Owner->EvaluateAIAbilityEligibility(Component, ExpectedResourceGeneration, ExpectedAIInstanceGeneration)
            : FGamePlatformResult::Failure(TEXT("AIControllerUnavailable"), TEXT("AI资格拥有者已失效。"));
    }
private:
    TWeakObjectPtr<AGamePlatformAIController> Controller;
    int32 ExpectedResourceGeneration = 0;
    int32 ExpectedAIInstanceGeneration = 0;
};

template <typename TInterface>
const TInterface* FindProvider(const AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return nullptr;
    }

    if (const TInterface* Direct = Cast<TInterface>(Actor))
    {
        return Direct;
    }

    TInlineComponentArray<UActorComponent*> Components;
    const_cast<AActor*>(Actor)->GetComponents(Components);

    for (const UActorComponent* Component : Components)
    {
        if (const TInterface* Provider =
            Cast<TInterface>(Component))
        {
            return Provider;
        }
    }

    return nullptr;
}

}

AGamePlatformAIController::AGamePlatformAIController()
{
    bReplicates = false;

    PlatformPerceptionComponent =
        CreateDefaultSubobject<UAIPerceptionComponent>(
            TEXT("GamePlatformAIPerception"));
    SetPerceptionComponent(*PlatformPerceptionComponent);

    SightConfig =
        CreateDefaultSubobject<UAISenseConfig_Sight>(
            TEXT("SightConfig"));
    HearingConfig =
        CreateDefaultSubobject<UAISenseConfig_Hearing>(
            TEXT("HearingConfig"));
    DamageConfig =
        CreateDefaultSubobject<UAISenseConfig_Damage>(
            TEXT("DamageConfig"));

    SightConfig->DetectionByAffiliation =
        FAISenseAffiliationFilter(true, true, true);
    HearingConfig->DetectionByAffiliation =
        FAISenseAffiliationFilter(true, true, true);

    PlatformPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
        this,
        &AGamePlatformAIController::HandleTargetPerceptionUpdated);
}

void AGamePlatformAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (!HasAuthority() || !IsValid(InPawn) || !GetWorld() || GetWorld()->bIsTearingDown ||
        !GamePlatformAIWorldPolicy::CanRun(GetWorld()->WorldType == EWorldType::Game || GetWorld()->WorldType == EWorldType::PIE,
            GetWorld()->GetNetMode() != NM_Client, IsRunningCommandlet()))
    {
        ResetRuntimeState(EGamePlatformAIError::NotAuthority);
        return;
    }

    StateComponent =
        InPawn->FindComponentByClass<UGamePlatformAIStateComponent>();
    AbilitySystemComponent =
        InPawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    CombatComponent =
        InPawn->FindComponentByClass<UGamePlatformCombatComponent>();

    if (!IsValid(StateComponent))
    {
        ResetRuntimeState(EGamePlatformAIError::MissingStateComponent);
        return;
    }

    if (AbilitySystemComponent)
    {
        AvatarBindingChangedHandle = AbilitySystemComponent->OnAvatarBindingChanged().AddUObject(this,
            &AGamePlatformAIController::HandleAbilityAvatarBindingChanged);
        // 权威AIController拥有本Pawn占有流程，只为Pawn自有且尚未绑定的ASC建立ActorInfo，不覆盖宿主已有绑定。
        if (AbilitySystemComponent->GetOwner() == InPawn &&
            ((!AbilitySystemComponent->GetAvatarActor() && !AbilitySystemComponent->GetOwnerActor()) ||
             (AbilitySystemComponent->GetAvatarActor() == InPawn && AbilitySystemComponent->GetOwnerActor() == InPawn &&
              AbilitySystemComponent->GetAvatarBindingSnapshot().AvatarGeneration <= 0)))
        { bOwnsAbilityActorInfo = AbilitySystemComponent->BindAbilityActorInfo(InPawn, InPawn); }
        RefreshAIActivationGate();
    }
    HomeLocation = InPawn->GetActorLocation();
    bStoppedForDeath = false;
    LastError = EGamePlatformAIError::None;

    BindCombatSignals();
    BeginDefinitionLoad();
}

void AGamePlatformAIController::OnUnPossess()
{
    ClearAIActivationGate(true);
    StopBrain(TEXT("UnPossess"));
    StopDecisionTimer();
    StopMovement();
    UnbindCombatSignals();

    ReleaseResourceLeases();

    if (PlatformPerceptionComponent)
    {
        PlatformPerceptionComponent->ForgetAll();
    }

    for (TPair<TWeakObjectPtr<AActor>, FGamePlatformAITargetCandidate>& Pair : Candidates)
    {
        if (AActor* Actor = Pair.Key.Get())
        {
            Actor->OnDestroyed.RemoveDynamic(
                this,
                &AGamePlatformAIController::HandleCandidateDestroyed);
        }
    }

    Candidates.Reset();
    CurrentTarget.Reset();
    ActiveDefinition = nullptr;
    AbilitySystemComponent = nullptr;
    CombatComponent = nullptr;

    if (StateComponent && StateComponent->GetOwner() &&
        StateComponent->GetOwner()->HasAuthority())
    {
        StateComponent->AdvanceServerGeneration();
    }
    StateComponent = nullptr;

    Super::OnUnPossess();
}

void AGamePlatformAIController::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    ClearAIActivationGate(true);
    StopBrain(TEXT("EndPlay"));
    StopDecisionTimer();
    UnbindCombatSignals(); ReleaseResourceLeases(); ActiveDefinition = nullptr;
    Super::EndPlay(EndPlayReason);
}

bool AGamePlatformAIController::TryAttackCurrentTarget()
{
    if (!HasAuthority() ||
        !ActiveDefinition ||
        !AbilitySystemComponent ||
        !CurrentTarget.IsValid() ||
        IsSelfDead() ||
        IsSelfStunned() ||
        IsSelfSilenced())
    {
        return false;
    }

    const double Now = GetServerTimeSeconds();
    if (Now < NextAttackTime)
    {
        return false;
    }

    const FGameplayTag AbilityTag =
        ActiveDefinition->PrimaryAbilityTag;
    if (!AbilityTag.IsValid())
    {
        LastError = EGamePlatformAIError::AbilityUnavailable;
        return false;
    }

    AActor* Target = CurrentTarget.Get();
    const FGamePlatformAITargetCandidate* Candidate =
        Candidates.Find(CurrentTarget);

    if (!Candidate ||
        !IsCandidateEligible(*Candidate) ||
        !IsValid(Target))
    {
        LastError = EGamePlatformAIError::NoValidTarget;
        return false;
    }

    const float Distance =
        FVector::Distance(
            GetPawn()->GetActorLocation(),
            Target->GetActorLocation());

    if (Distance > ActiveDefinition->AttackRange)
    {
        return false;
    }

    FGameplayTagContainer AbilityTags;
    AbilityTags.AddTag(AbilityTag);

    // AI的实际攻击入口也先读取中立Gate，外部原生能力不能绕过本Controller权威资格。
    if (!AbilitySystemComponent->EvaluateActivationEligibility().IsSuccess())
    { LastError = EGamePlatformAIError::AbilityUnavailable; return false; }
    const bool bActivationAttempted =
        AbilitySystemComponent->TryActivateAbilitiesByTag(
            AbilityTags,
            false);

    NextAttackTime =
        Now +
        FMath::Max(
            0.05f,
            ActiveDefinition->UpdateProfile.AttackRetryBackoff);

    LastError =
        bActivationAttempted
            ? EGamePlatformAIError::None
            : EGamePlatformAIError::AbilityUnavailable;

    return bActivationAttempted;
}

void AGamePlatformAIController::ForceDecisionUpdate()
{
    if (HasAuthority())
    {
        EvaluateDecision();
    }
}

void AGamePlatformAIController::HandleTargetPerceptionUpdated(
    AActor* Actor,
    FAIStimulus Stimulus)
{
    if (!HasAuthority() || !ActiveDefinition || !IsValid(Actor))
    {
        return;
    }

    const FAISenseID SightId =
        UAISense::GetSenseID<UAISense_Sight>();
    const FAISenseID HearingId =
        UAISense::GetSenseID<UAISense_Hearing>();
    const FAISenseID DamageId =
        UAISense::GetSenseID<UAISense_Damage>();

    const bool bSensed = Stimulus.WasSuccessfullySensed();
    const bool bSight = Stimulus.Type == SightId;
    const bool bHearing = Stimulus.Type == HearingId;
    const bool bDamage = Stimulus.Type == DamageId;

    if (bSight || bHearing || bDamage)
    {
        UpsertCandidate(
            Actor,
            Stimulus.StimulusLocation,
            bSight && bSensed,
            bHearing && bSensed,
            bDamage && bSensed);
        EvaluateDecision();
    }
}

void AGamePlatformAIController::HandleCandidateDestroyed(
    AActor* DestroyedActor)
{
    if (!DestroyedActor)
    {
        return;
    }

    Candidates.Remove(
        TWeakObjectPtr<AActor>(DestroyedActor));

    if (CurrentTarget.Get() == DestroyedActor)
    {
        SetCurrentTarget(nullptr);
        EvaluateDecision();
    }
}

void AGamePlatformAIController::HandleCombatEvent(
    const FGamePlatformCombatEvent& Event)
{
    if (!HasAuthority() || !GetPawn())
    {
        return;
    }

    if (Event.TargetActor == GetPawn() &&
        Event.EventType == EGamePlatformCombatEventType::Death)
    {
        bStoppedForDeath = true;
        StopMovement();
        StopBrain(TEXT("CombatDeath"));
        SetCurrentTarget(nullptr);
        for (TPair<TWeakObjectPtr<AActor>, FGamePlatformAITargetCandidate>& Pair : Candidates)
        {
            if (AActor* CandidateActor = Pair.Key.Get())
            {
                CandidateActor->OnDestroyed.RemoveDynamic(
                    this,
                    &AGamePlatformAIController::HandleCandidateDestroyed);
            }
        }
        Candidates.Reset();
        if (PlatformPerceptionComponent)
        {
            PlatformPerceptionComponent->ForgetAll();
        }
        SetPublicState(EGamePlatformAIPublicState::Dead);

        if (UBlackboardComponent* LocalBlackboard = GetBlackboardComponent())
        {
            LocalBlackboard->SetValueAsBool(
                GamePlatformAIBlackboardKeys::IsDead,
                true);
        }
        return;
    }

    if (Event.TargetActor == GetPawn() &&
        Event.EventType == EGamePlatformCombatEventType::Damage &&
        IsValid(Event.SourceActor))
    {
        UpsertCandidate(
            Event.SourceActor,
            Event.SourceActor->GetActorLocation(),
            false,
            false,
            true);
        EvaluateDecision();
    }

    if (Event.TargetActor == GetPawn() &&
        Event.EventType == EGamePlatformCombatEventType::RespawnReset)
    {
        bStoppedForDeath = false;
        if (StateComponent)
        {
            StateComponent->InitializeServerState(
                ActiveDefinition
                    ? ActiveDefinition->AIDefinitionId
                    : NAME_None,
                Event.TargetAvatarGeneration);
        }

        if (BrainComponent)
        {
            BrainComponent->RestartLogic();
        }
        bBrainReady =
            ActiveDefinition != nullptr &&
            BrainComponent != nullptr;

        if (UBlackboardComponent* LocalBlackboard = GetBlackboardComponent())
        {
            LocalBlackboard->SetValueAsBool(
                GamePlatformAIBlackboardKeys::IsDead,
                false);
            LocalBlackboard->ClearValue(
                GamePlatformAIBlackboardKeys::TargetActor);
            LocalBlackboard->SetValueAsBool(
                GamePlatformAIBlackboardKeys::CanAttack,
                false);
        }

        if (PlatformPerceptionComponent)
        {
            PlatformPerceptionComponent->ForgetAll();
        }
        Candidates.Reset();
        CurrentTarget.Reset();
        ResourceAIInstanceGeneration = GetCurrentGeneration();
        RefreshAIActivationGate();
        StartDecisionTimer();
        EvaluateDecision();
    }
}

void AGamePlatformAIController::HandleStunTagChanged(
    const FGameplayTag Tag,
    int32 NewCount)
{
    if (!HasAuthority() || Tag != GamePlatformCombatTags::Control_Stun)
    {
        return;
    }

    if (NewCount > 0)
    {
        StopMovement();
        if (BrainComponent && BrainComponent->IsRunning())
        {
            BrainComponent->PauseLogic(TEXT("Combat.Stun"));
        }
        SetPublicState(EGamePlatformAIPublicState::Controlled);
    }
    else if (!IsSelfDead() && BrainComponent)
    {
        if (BrainComponent->IsPaused())
        {
            BrainComponent->ResumeLogic(TEXT("Combat.Stun.Removed"));
        }
        EvaluateDecision();
    }
}

void AGamePlatformAIController::HandleSilenceTagChanged(
    const FGameplayTag Tag,
    int32 NewCount)
{
    if (HasAuthority() &&
        Tag == GamePlatformCombatTags::Control_Silence &&
        NewCount == 0)
    {
        NextAttackTime = 0.0;
    }
}

void AGamePlatformAIController::BeginDefinitionLoad()
{
    if (!StateComponent)
    {
        ResetRuntimeState(EGamePlatformAIError::MissingStateComponent);
        return;
    }

    TSoftObjectPtr<UGamePlatformAIDefinition> Definition =
        StateComponent->GetDefinitionAsset();

    if (Definition.IsNull())
    {
        Definition = DefaultDefinition;
    }

    const FSoftObjectPath Path = Definition.ToSoftObjectPath();
    if (!Path.IsValid())
    {
        ResetRuntimeState(EGamePlatformAIError::InvalidDefinition);
        return;
    }

    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data) { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
    const int32 Generation = ++ResourceRequestGeneration;
    ResourceAIInstanceGeneration = GetCurrentGeneration(); DefinitionResourcePath = Path;
    FGamePlatformResult Accepted;
    DefinitionResourceLease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, this,
        [WeakThis = TWeakObjectPtr<AGamePlatformAIController>(this), Generation](const auto& Lease, const auto& Result)
        {
            auto* Self = WeakThis.Get();
            if (!Self || Generation != Self->ResourceRequestGeneration || (Lease.LeaseId != Self->DefinitionResourceLease.LeaseId || Lease.ScopeId != Self->DefinitionResourceLease.ScopeId || Lease.Generation != Self->DefinitionResourceLease.Generation)) { return; }
            if (!Result.IsSuccess()) { Self->ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
            Self->HandleDefinitionLoaded(Generation);
        }, Accepted);
    if (!Accepted.IsSuccess()) { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); }

}

void AGamePlatformAIController::HandleDefinitionLoaded(
    int32 ExpectedGeneration)
{
    if (!StateComponent ||
        ExpectedGeneration != ResourceRequestGeneration || ResourceAIInstanceGeneration != GetCurrentGeneration())
    {
        return;
    }

    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data || Data->GetLeaseState(DefinitionResourceLease) != EGamePlatformDataRequestState::Succeeded)
    { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
    // 完成时只读取本次租约预检过的路径；配置已切换却未更新代次也不能串用另一个定义。
    auto CurrentDefinition = StateComponent->GetDefinitionAsset();
    if (CurrentDefinition.IsNull()) { CurrentDefinition = DefaultDefinition; }
    if (CurrentDefinition.ToSoftObjectPath() != DefinitionResourcePath)
    { ResetRuntimeState(EGamePlatformAIError::InvalidDefinition); return; }
    UGamePlatformAIDefinition* Loaded = Cast<UGamePlatformAIDefinition>(DefinitionResourcePath.ResolveObject());

    if (!IsValid(Loaded) || !InitializeDefinition(*Loaded))
    {
        ResetRuntimeState(EGamePlatformAIError::InvalidDefinition);
        return;
    }

    BeginBrainAssetLoad(ExpectedGeneration);
}

bool AGamePlatformAIController::InitializeDefinition(
    UGamePlatformAIDefinition& Definition)
{
    FText Reason;
    if (!Definition.ValidateDefinition(Reason))
    {
        return false;
    }

    if (Definition.BrainType == EGamePlatformAIBrainType::StateTree)
    {
        LastError = EGamePlatformAIError::UnsupportedBrain;
        return false;
    }

    if (Definition.BrainType != EGamePlatformAIBrainType::BehaviorTree)
    {
        LastError = EGamePlatformAIError::UnsupportedBrain;
        return false;
    }

    ActiveDefinition = &Definition;
    ConfigurePerception(Definition);

    if (StateComponent)
    {
        StateComponent->InitializeServerState(
            Definition.AIDefinitionId,
            GetCurrentGeneration());
    }

    return true;
}

void AGamePlatformAIController::BeginBrainAssetLoad(
    int32 ExpectedGeneration)
{
    if (!ActiveDefinition)
    {
        ResetRuntimeState(EGamePlatformAIError::InvalidDefinition);
        return;
    }

    TArray<FSoftObjectPath> Assets;
    ActiveDefinition->GetReferencedAssetPaths(Assets);

    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data) { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
    FGamePlatformResult Accepted;
    BrainResourceLease = Data->AcquireResources(Assets, EGamePlatformDataLifetime::World, this,
        [WeakThis = TWeakObjectPtr<AGamePlatformAIController>(this), ExpectedGeneration](const auto& Lease, const auto& Result)
        {
            auto* Self = WeakThis.Get();
            if (!Self || ExpectedGeneration != Self->ResourceRequestGeneration || (Lease.LeaseId != Self->BrainResourceLease.LeaseId || Lease.ScopeId != Self->BrainResourceLease.ScopeId || Lease.Generation != Self->BrainResourceLease.Generation)) { return; }
            if (!Result.IsSuccess()) { Self->ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
            Self->HandleBrainAssetsLoaded(ExpectedGeneration);
        }, Accepted);
    if (!Accepted.IsSuccess()) { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); }

}

void AGamePlatformAIController::HandleBrainAssetsLoaded(
    int32 ExpectedGeneration)
{
    if (!ActiveDefinition ||
        !StateComponent ||
        ExpectedGeneration != ResourceRequestGeneration || ResourceAIInstanceGeneration != GetCurrentGeneration())
    {
        return;
    }

    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data || Data->GetLeaseState(DefinitionResourceLease) != EGamePlatformDataRequestState::Succeeded ||
        Data->GetLeaseState(BrainResourceLease) != EGamePlatformDataRequestState::Succeeded)
    { ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed); return; }
    if (!StartBehaviorTreeBrain(*ActiveDefinition))
    {
        ResetRuntimeState(EGamePlatformAIError::AssetLoadFailed);
        return;
    }

    bBrainReady = true;
    RefreshAIActivationGate();
    LastError = EGamePlatformAIError::None;
    StartDecisionTimer();
    EvaluateDecision();
}

void AGamePlatformAIController::ConfigurePerception(
    const UGamePlatformAIDefinition& Definition)
{
    const FGamePlatformAIPerceptionProfile& Profile =
        Definition.PerceptionProfile;

    SightConfig->SightRadius = Profile.SightRadius;
    SightConfig->LoseSightRadius = Profile.LoseSightRadius;
    SightConfig->PeripheralVisionAngleDegrees =
        Profile.PeripheralVisionHalfAngleDegrees;
    SightConfig->SetMaxAge(Profile.MaxAge);
    SightConfig->DetectionByAffiliation =
        FAISenseAffiliationFilter(true, true, true);

    PlatformPerceptionComponent->ConfigureSense(*SightConfig);
    PlatformPerceptionComponent->SetDominantSense(
        SightConfig->GetSenseImplementation());

    HearingConfig->HearingRange = Profile.HearingRange;
    HearingConfig->SetMaxAge(Profile.MaxAge);
    HearingConfig->DetectionByAffiliation =
        FAISenseAffiliationFilter(true, true, true);
    PlatformPerceptionComponent->ConfigureSense(*HearingConfig);
    PlatformPerceptionComponent->SetSenseEnabled(
        UAISense_Hearing::StaticClass(),
        Profile.bEnableHearing);

    DamageConfig->SetMaxAge(Profile.MaxAge);
    PlatformPerceptionComponent->ConfigureSense(*DamageConfig);
    PlatformPerceptionComponent->SetSenseEnabled(
        UAISense_Damage::StaticClass(),
        Profile.bEnableDamageSense);

    PlatformPerceptionComponent->RequestStimuliListenerUpdate();
}

bool AGamePlatformAIController::StartBehaviorTreeBrain(
    const UGamePlatformAIDefinition& Definition)
{
    UBehaviorTree* BehaviorTree =
        Cast<UBehaviorTree>(
            Definition.BehaviorTreeAsset.ResolveObject());
    UBlackboardData* BlackboardData =
        Cast<UBlackboardData>(
            Definition.BlackboardAsset.ResolveObject());

    if (!BehaviorTree || !BlackboardData)
    {
        return false;
    }

    UBlackboardComponent* LocalBlackboard = nullptr;
    if (!UseBlackboard(BlackboardData, LocalBlackboard) ||
        !LocalBlackboard)
    {
        return false;
    }

    LocalBlackboard->SetValueAsVector(
        GamePlatformAIBlackboardKeys::HomeLocation,
        HomeLocation);
    LocalBlackboard->SetValueAsBool(
        GamePlatformAIBlackboardKeys::IsDead,
        false);
    LocalBlackboard->SetValueAsBool(
        GamePlatformAIBlackboardKeys::CanAttack,
        false);

    return RunBehaviorTree(BehaviorTree);
}

void AGamePlatformAIController::StopBrain(const FString& Reason)
{
    if (BrainComponent)
    {
        BrainComponent->StopLogic(Reason);
    }

    bBrainReady = false;
}

void AGamePlatformAIController::StartDecisionTimer()
{
    StopDecisionTimer();

    if (!ActiveDefinition || !GetWorld())
    {
        return;
    }

    const float Interval =
        FMath::Max(
            0.05f,
            ActiveDefinition->UpdateProfile.DecisionInterval);

    GetWorld()->GetTimerManager().SetTimer(
        DecisionTimer,
        this,
        &AGamePlatformAIController::EvaluateDecision,
        Interval,
        true);
}

void AGamePlatformAIController::StopDecisionTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DecisionTimer);
    }
}

void AGamePlatformAIController::EvaluateDecision()
{
    if (!HasAuthority() ||
        !bBrainReady ||
        !ActiveDefinition ||
        !GetPawn() ||
        bStoppedForDeath)
    {
        return;
    }

    if (IsSelfDead())
    {
        StopMovement();
        StopBrain(TEXT("DeadTag"));
        SetCurrentTarget(nullptr);
        SetPublicState(EGamePlatformAIPublicState::Dead);
        return;
    }

    if (IsSelfStunned())
    {
        StopMovement();
        SetPublicState(EGamePlatformAIPublicState::Controlled);
        return;
    }

    PruneCandidates();
    AActor* BestTarget = SelectBestTarget();
    SetCurrentTarget(BestTarget);

    if (BestTarget)
    {
        UpdateTargetBehavior(*BestTarget);
    }
    else
    {
        UpdateNoTargetBehavior();
    }
}

void AGamePlatformAIController::PruneCandidates()
{
    if (!ActiveDefinition)
    {
        Candidates.Reset();
        return;
    }

    const double Now = GetServerTimeSeconds();
    const double Memory =
        FMath::Max(
            0.1f,
            ActiveDefinition->TargetSelectionProfile.MemorySeconds);

    TArray<TWeakObjectPtr<AActor>> ToRemove;
    for (const TPair<TWeakObjectPtr<AActor>, FGamePlatformAITargetCandidate>& Pair : Candidates)
    {
        if (!Pair.Key.IsValid() ||
            Now - Pair.Value.LastSensedTime > Memory ||
            !IsCandidateEligible(Pair.Value))
        {
            ToRemove.Add(Pair.Key);
        }
    }

    for (const TWeakObjectPtr<AActor>& Key : ToRemove)
    {
        if (AActor* Actor = Key.Get())
        {
            Actor->OnDestroyed.RemoveDynamic(
                this,
                &AGamePlatformAIController::HandleCandidateDestroyed);
        }
        Candidates.Remove(Key);
    }

    const int32 MaxCandidates =
        FMath::Max(
            1,
            ActiveDefinition->TargetSelectionProfile.MaxCandidates);

    if (Candidates.Num() <= MaxCandidates) { return; }
    // 旧实现每移除一个候选都扫描全表，过量N-K时成本接近平方；一次堆选择只需O(N log K)。
    TArray<FGamePlatformAITargetCandidate> RankedCandidates;
    Candidates.GenerateValueArray(RankedCandidates);
    auto* Removed = GamePlatformAICandidateRetention::SelectRetained(
        RankedCandidates.GetData(), RankedCandidates.GetData() + RankedCandidates.Num(),
        static_cast<std::size_t>(MaxCandidates), [](const auto& A, const auto& B)
        {
            if (A.LastSensedTime != B.LastSensedTime) { return A.LastSensedTime > B.LastSensedTime; }
            // 同时间按稳定EntityId显式排序，不依赖TMap扫描顺序。
            if (A.EntityId.A != B.EntityId.A) { return A.EntityId.A < B.EntityId.A; }
            if (A.EntityId.B != B.EntityId.B) { return A.EntityId.B < B.EntityId.B; }
            if (A.EntityId.C != B.EntityId.C) { return A.EntityId.C < B.EntityId.C; }
            return A.EntityId.D < B.EntityId.D;
        });
    for (; Removed != RankedCandidates.GetData() + RankedCandidates.Num(); ++Removed)
    {
        if (AActor* Actor = Removed->Actor.Get())
        { Actor->OnDestroyed.RemoveDynamic(this, &AGamePlatformAIController::HandleCandidateDestroyed); }
        Candidates.Remove(Removed->Actor);
    }
}

void AGamePlatformAIController::UpsertCandidate(
    AActor* Actor,
    const FVector& Location,
    bool bVisible,
    bool bHeard,
    bool bDamageSource)
{
    if (!IsValid(Actor) || Actor == GetPawn() || !ActiveDefinition)
    {
        return;
    }

    const IGamePlatformAITargetEligibilityProvider* Provider =
        FindProvider<IGamePlatformAITargetEligibilityProvider>(Actor);

    if (!Provider ||
        !Provider->IsEligibleAsAITarget(GetPawn()))
    {
        return;
    }

    const TWeakObjectPtr<AActor> Key(Actor);
    const bool bNew = !Candidates.Contains(Key);
    FGamePlatformAITargetCandidate& Candidate =
        Candidates.FindOrAdd(Key);

    Candidate.Actor = Actor;
    Candidate.EntityId = Provider->GetAITargetEntityId();
    Candidate.Generation = Provider->GetAITargetGeneration();
    Candidate.LastKnownLocation =
        Location.IsNearlyZero()
            ? Actor->GetActorLocation()
            : Location;
    Candidate.LastSensedTime = GetServerTimeSeconds();

    if (bVisible)
    {
        Candidate.bVisible = true;
    }
    else if (!bHeard && !bDamageSource)
    {
        Candidate.bVisible = false;
    }

    Candidate.bHeard = Candidate.bHeard || bHeard;
    Candidate.bDamageSource =
        Candidate.bDamageSource || bDamageSource;

    if (bNew)
    {
        Actor->OnDestroyed.AddUniqueDynamic(
            this,
            &AGamePlatformAIController::HandleCandidateDestroyed);
    }

    PruneCandidates();
}

AActor* AGamePlatformAIController::SelectBestTarget()
{
    if (!ActiveDefinition || !GetPawn())
    {
        return nullptr;
    }

    const FGamePlatformAITargetCandidate* Best = nullptr;
    FGamePlatformAITargetSortKey BestKey;

    for (const TPair<TWeakObjectPtr<AActor>, FGamePlatformAITargetCandidate>& Pair : Candidates)
    {
        const FGamePlatformAITargetCandidate& Candidate = Pair.Value;
        AActor* Actor = Candidate.Actor.Get();
        if (!Actor)
        {
            continue;
        }

        FGamePlatformAITargetSortKey CandidateKey;
        CandidateKey.bEligible = IsCandidateEligible(Candidate);
        CandidateKey.bVisible = Candidate.bVisible;
        CandidateKey.DistanceSquared =
            FVector::DistSquared(
                GetPawn()->GetActorLocation(),
                Actor->GetActorLocation());
        CandidateKey.LastSensedTime = Candidate.LastSensedTime;
        CandidateKey.EntityId = Candidate.EntityId;

        if (!CandidateKey.bEligible)
        {
            continue;
        }

        if (!Best ||
            FGamePlatformAITargetSelectionRules::IsBetter(
                CandidateKey,
                BestKey,
                ActiveDefinition->TargetSelectionProfile.bPreferVisibleTargets))
        {
            Best = &Candidate;
            BestKey = CandidateKey;
        }
    }

    return Best ? Best->Actor.Get() : nullptr;
}

bool AGamePlatformAIController::IsCandidateEligible(
    const FGamePlatformAITargetCandidate& Candidate) const
{
    AActor* Actor = Candidate.Actor.Get();
    if (!Actor ||
        !GetPawn() ||
        Actor->GetWorld() != GetWorld())
    {
        return false;
    }

    const IGamePlatformAITargetEligibilityProvider* Provider =
        FindProvider<IGamePlatformAITargetEligibilityProvider>(Actor);

    if (!Provider ||
        !Provider->IsEligibleAsAITarget(GetPawn()) ||
        Provider->GetAITargetEntityId() != Candidate.EntityId ||
        Provider->GetAITargetGeneration() != Candidate.Generation)
    {
        return false;
    }

    if (UGamePlatformCombatComponent* TargetCombat =
        Actor->FindComponentByClass<UGamePlatformCombatComponent>())
    {
        if (TargetCombat->IsCombatDead())
        {
            return false;
        }
    }

    if (ActiveDefinition &&
        ActiveDefinition->HomePolicy.LeashRadius > 0.0f &&
        FVector::DistSquared(
            HomeLocation,
            Actor->GetActorLocation()) >
            FMath::Square(
                ActiveDefinition->HomePolicy.LeashRadius))
    {
        return false;
    }

    return true;
}

void AGamePlatformAIController::SetCurrentTarget(AActor* NewTarget)
{
    if (CurrentTarget.Get() == NewTarget)
    {
        UpdateBlackboardForTarget(NewTarget);
        return;
    }

    CurrentTarget = NewTarget;

    if (StateComponent)
    {
        FGuid EntityId;
        if (NewTarget)
        {
            if (const IGamePlatformAITargetEligibilityProvider* Provider =
                FindProvider<IGamePlatformAITargetEligibilityProvider>(
                    NewTarget))
            {
                EntityId = Provider->GetAITargetEntityId();
            }
        }
        StateComponent->SetServerTargetEntityId(EntityId);
    }

    UpdateBlackboardForTarget(NewTarget);
}

void AGamePlatformAIController::UpdateBlackboardForTarget(
    AActor* Target)
{
    UBlackboardComponent* LocalBlackboard =
        GetBlackboardComponent();
    if (!LocalBlackboard)
    {
        return;
    }

    LocalBlackboard->SetValueAsObject(
        GamePlatformAIBlackboardKeys::TargetActor,
        Target);

    if (!Target)
    {
        LocalBlackboard->SetValueAsBool(
            GamePlatformAIBlackboardKeys::HasLineOfSight,
            false);
        LocalBlackboard->SetValueAsBool(
            GamePlatformAIBlackboardKeys::CanAttack,
            false);
        return;
    }

    const FGamePlatformAITargetCandidate* Candidate =
        Candidates.Find(TWeakObjectPtr<AActor>(Target));

    const bool bVisible = Candidate && Candidate->bVisible;
    LocalBlackboard->SetValueAsBool(
        GamePlatformAIBlackboardKeys::HasLineOfSight,
        bVisible);

    LocalBlackboard->SetValueAsVector(
        GamePlatformAIBlackboardKeys::LastKnownTargetLocation,
        Candidate
            ? Candidate->LastKnownLocation
            : Target->GetActorLocation());
}

void AGamePlatformAIController::UpdateNoTargetBehavior()
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn || !ActiveDefinition)
    {
        return;
    }

    UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
    if (!LocalBlackboard)
    {
        return;
    }

    LocalBlackboard->SetValueAsBool(
        GamePlatformAIBlackboardKeys::CanAttack,
        false);

    const float DistanceHomeSq =
        FVector::DistSquared(
            ControlledPawn->GetActorLocation(),
            HomeLocation);

    const float ReturnRadius =
        FMath::Max(
            ActiveDefinition->HomePolicy.ReturnHomeAcceptanceRadius,
            1.0f);

    if (DistanceHomeSq > FMath::Square(ReturnRadius * 2.0f))
    {
        RequestReturnHome();
        return;
    }

    if (GetMoveStatus() == EPathFollowingStatus::Idle &&
        GetServerTimeSeconds() >= NextPatrolSelectionTime)
    {
        if (!RequestPatrolGoal())
        {
            SetPublicState(EGamePlatformAIPublicState::Idle);
        }
    }
}

void AGamePlatformAIController::UpdateTargetBehavior(AActor& Target)
{
    if (!ActiveDefinition || !GetPawn())
    {
        return;
    }

    const FGamePlatformAITargetCandidate* Candidate =
        Candidates.Find(TWeakObjectPtr<AActor>(&Target));

    if (!Candidate)
    {
        return;
    }

    const float Distance =
        FVector::Distance(
            GetPawn()->GetActorLocation(),
            Target.GetActorLocation());

    if (!Candidate->bVisible && Candidate->bHeard)
    {
        if (UBlackboardComponent* LocalBlackboard =
            GetBlackboardComponent())
        {
            LocalBlackboard->SetValueAsBool(
                GamePlatformAIBlackboardKeys::CanAttack,
                false);
            LocalBlackboard->SetValueAsVector(
                GamePlatformAIBlackboardKeys::GoalLocation,
                Candidate->LastKnownLocation);
            LocalBlackboard->SetValueAsVector(
                GamePlatformAIBlackboardKeys::LastKnownTargetLocation,
                Candidate->LastKnownLocation);
        }

        LastMoveGoal = Candidate->LastKnownLocation;
        SetPublicState(
            EGamePlatformAIPublicState::Investigating,
            GamePlatformAITags::Movement_Investigate);
        return;
    }

    const bool bCanAttack =
        Candidate->bVisible &&
        Distance <= ActiveDefinition->AttackRange &&
        !IsSelfSilenced();

    if (UBlackboardComponent* LocalBlackboard =
        GetBlackboardComponent())
    {
        LocalBlackboard->SetValueAsBool(
            GamePlatformAIBlackboardKeys::CanAttack,
            bCanAttack);
        LocalBlackboard->SetValueAsVector(
            GamePlatformAIBlackboardKeys::GoalLocation,
            Target.GetActorLocation());
    }

    if (bCanAttack)
    {
        StopMovement();
        SetPublicState(
            EGamePlatformAIPublicState::Attacking,
            FGameplayTag(),
            GamePlatformAITags::Combat_Attack);
    }
    else
    {
        RequestChase(Target);
    }
}

bool AGamePlatformAIController::RequestPatrolGoal()
{
    if (!ActiveDefinition || !GetWorld())
    {
        return false;
    }

    UGamePlatformNavigationWorldSubsystem* Navigation =
        GetWorld()->GetSubsystem<UGamePlatformNavigationWorldSubsystem>();

    if (!Navigation)
    {
        LastError = EGamePlatformAIError::NavigationUnavailable;
        return false;
    }

    FVector PatrolLocation = FVector::ZeroVector;
    EGamePlatformNavigationError NavigationError =
        EGamePlatformNavigationError::None;

    const bool bFound =
        Navigation->FindRandomReachablePoint(
            HomeLocation,
            ActiveDefinition->HomePolicy.PatrolRadius,
            ActiveDefinition->NavigationProfileId,
            NAME_None,
            PatrolLocation,
            NavigationError);

    if (!bFound)
    {
        LastError = EGamePlatformAIError::NavigationUnavailable;
        NextPatrolSelectionTime =
            GetServerTimeSeconds() + 1.0;
        return false;
    }

    LastMoveGoal = PatrolLocation;

    if (UBlackboardComponent* LocalBlackboard =
        GetBlackboardComponent())
    {
        LocalBlackboard->SetValueAsVector(
            GamePlatformAIBlackboardKeys::GoalLocation,
            PatrolLocation);
    }

    SetPublicState(
        EGamePlatformAIPublicState::Moving,
        GamePlatformAITags::Movement_Patrol);

    NextPatrolSelectionTime =
        GetServerTimeSeconds() + 1.0;
    return true;
}

void AGamePlatformAIController::RequestReturnHome()
{
    if (!ActiveDefinition)
    {
        return;
    }

    if (UBlackboardComponent* LocalBlackboard =
        GetBlackboardComponent())
    {
        LocalBlackboard->SetValueAsVector(
            GamePlatformAIBlackboardKeys::GoalLocation,
            HomeLocation);
    }

    SetPublicState(
        EGamePlatformAIPublicState::Moving,
        GamePlatformAITags::Movement_ReturnHome);
}

void AGamePlatformAIController::RequestChase(AActor& Target)
{
    if (!ActiveDefinition || !GetPawn())
    {
        return;
    }

    const double Now = GetServerTimeSeconds();
    const FVector TargetLocation = Target.GetActorLocation();

    const bool bMovedEnough =
        FVector::DistSquared(
            LastMoveGoal,
            TargetLocation) >=
        FMath::Square(
            ActiveDefinition->UpdateProfile.MoveRefreshDistance);

    const bool bRefreshTime =
        LastMoveRequestTime < 0.0 ||
        Now - LastMoveRequestTime >=
            ActiveDefinition->UpdateProfile.MoveRefreshInterval;

    if (bMovedEnough || bRefreshTime)
    {
        LastMoveGoal = TargetLocation;
        LastMoveRequestTime = Now;

        if (UBlackboardComponent* LocalBlackboard =
            GetBlackboardComponent())
        {
            LocalBlackboard->SetValueAsVector(
                GamePlatformAIBlackboardKeys::GoalLocation,
                TargetLocation);
        }
    }

    SetPublicState(
        EGamePlatformAIPublicState::Chasing,
        GamePlatformAITags::Movement_Chase);
}

void AGamePlatformAIController::SetPublicState(
    EGamePlatformAIPublicState NewState,
    const FGameplayTag& MovementIntent,
    const FGameplayTag& CombatIntent)
{
    if (!StateComponent)
    {
        return;
    }

    StateComponent->SetServerPublicState(NewState);
    StateComponent->SetServerIntentTags(
        MovementIntent,
        CombatIntent);
}

bool AGamePlatformAIController::IsSelfDead() const
{
    return CombatComponent && CombatComponent->IsCombatDead();
}

bool AGamePlatformAIController::IsSelfStunned() const
{
    return AbilitySystemComponent &&
        AbilitySystemComponent->HasMatchingGameplayTag(
            GamePlatformCombatTags::Control_Stun);
}

bool AGamePlatformAIController::IsSelfSilenced() const
{
    return AbilitySystemComponent &&
        AbilitySystemComponent->HasMatchingGameplayTag(
            GamePlatformCombatTags::Control_Silence);
}

int32 AGamePlatformAIController::GetCurrentGeneration() const
{
    return StateComponent
        ? StateComponent->GetSnapshot().AIInstanceGeneration
        : 0;
}

double AGamePlatformAIController::GetServerTimeSeconds() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AGamePlatformAIController::BindCombatSignals()
{
    if (CombatComponent)
    {
        CombatComponent->OnCombatEvent.AddUniqueDynamic(
            this,
            &AGamePlatformAIController::HandleCombatEvent);
    }

    if (AbilitySystemComponent)
    {
        StunTagHandle =
            AbilitySystemComponent->RegisterGameplayTagEvent(
                GamePlatformCombatTags::Control_Stun,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(
                    this,
                    &AGamePlatformAIController::HandleStunTagChanged);

        SilenceTagHandle =
            AbilitySystemComponent->RegisterGameplayTagEvent(
                GamePlatformCombatTags::Control_Silence,
                EGameplayTagEventType::NewOrRemoved)
                .AddUObject(
                    this,
                    &AGamePlatformAIController::HandleSilenceTagChanged);
    }
}

void AGamePlatformAIController::UnbindCombatSignals()
{
    if (CombatComponent)
    {
        CombatComponent->OnCombatEvent.RemoveDynamic(
            this,
            &AGamePlatformAIController::HandleCombatEvent);
    }

    if (AbilitySystemComponent)
    {
        if (StunTagHandle.IsValid())
        {
            AbilitySystemComponent->UnregisterGameplayTagEvent(
                StunTagHandle,
                GamePlatformCombatTags::Control_Stun,
                EGameplayTagEventType::NewOrRemoved);
        }

        if (SilenceTagHandle.IsValid())
        {
            AbilitySystemComponent->UnregisterGameplayTagEvent(
                SilenceTagHandle,
                GamePlatformCombatTags::Control_Silence,
                EGameplayTagEventType::NewOrRemoved);
        }
    }

    StunTagHandle.Reset();
    SilenceTagHandle.Reset();
}

void AGamePlatformAIController::ResetRuntimeState(
    EGamePlatformAIError Error)
{
    LastError = Error;
    ClearAIActivationGate(false);
    StopDecisionTimer();
    StopMovement();
    StopBrain(TEXT("ResetRuntimeState"));
    ReleaseResourceLeases(); ActiveDefinition = nullptr;

    if (StateComponent)
    {
        StateComponent->SetServerPublicState(
            EGamePlatformAIPublicState::Disabled);
    }
}

void AGamePlatformAIController::ReleaseResourceLeases()
{
    check(IsInGameThread()); ++ResourceRequestGeneration;
    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr)
    {
        if (BrainResourceLease.IsValid()) { Data->ReleaseResources(BrainResourceLease); }
        if (DefinitionResourceLease.IsValid()) { Data->ReleaseResources(DefinitionResourceLease); }
    }
    BrainResourceLease = {}; DefinitionResourceLease = {}; DefinitionResourcePath.Reset();
    ResourceAIInstanceGeneration = 0;
}

FGamePlatformResult AGamePlatformAIController::EvaluateAIAbilityEligibility(
    const UGamePlatformAbilitySystemComponent& Component, int32 ExpectedResourceGeneration, int32 ExpectedAIInstanceGeneration) const
{
    check(IsInGameThread());
    const APawn* ControlledPawn = GetPawn(); const UWorld* World = GetWorld();
    const bool bAuthorityWorld = HasAuthority() && ControlledPawn && ControlledPawn->HasAuthority() && World && !World->bIsTearingDown &&
        GamePlatformAIWorldPolicy::CanRun(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE,
            World->GetNetMode() != NM_Client, IsRunningCommandlet());
    const bool bOwnerCurrent = ControlledPawn && ControlledPawn->GetController() == this && ControlledPawn->GetWorld() == World &&
        AbilitySystemComponent == &Component && Component.GetWorld() == World && Component.GetAvatarActor() == ControlledPawn &&
        (Component.GetOwnerActor() == ControlledPawn || Component.GetOwnerActor() == this);
    auto* Instance = World ? World->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    const bool bResourcesHeld = Data && Data->GetLeaseState(DefinitionResourceLease) == EGamePlatformDataRequestState::Succeeded &&
        Data->GetLeaseState(BrainResourceLease) == EGamePlatformDataRequestState::Succeeded;
    const bool bDefinitionReady = StateComponent && StateComponent->GetOwner() == ControlledPawn && ActiveDefinition &&
        !ActiveDefinition->AIDefinitionId.IsNone() && StateComponent->GetSnapshot().AIDefinitionId == ActiveDefinition->AIDefinitionId &&
        bBrainReady && BrainComponent && BrainComponent->IsRunning() && ResourceAIInstanceGeneration == GetCurrentGeneration();
    if (!GamePlatformAIAbilityPolicy::CanActivate(bAuthorityWorld, bOwnerCurrent, bDefinitionReady, bResourcesHeld,
        !bStoppedForDeath && !IsSelfDead() && !IsSelfStunned() && !IsSelfSilenced(), GetCurrentGeneration(), ExpectedAIInstanceGeneration,
        ResourceRequestGeneration, ExpectedResourceGeneration))
    { return FGamePlatformResult::Failure(TEXT("AIActivationNotReady"), TEXT("AI当前权威占有、代次、定义、Brain或资源资格不满足。")); }
    return FGamePlatformResult::Success();
}

void AGamePlatformAIController::RefreshAIActivationGate()
{
    check(IsInGameThread());
    if (!AbilitySystemComponent) { return; }
    AbilitySystemComponent->ClearActivationGate(this);
    if (HasAuthority() && GetPawn() && GetPawn()->GetController() == this &&
        AbilitySystemComponent->GetAvatarBindingSnapshot().bBound && AbilitySystemComponent->GetAvatarActor() == GetPawn())
    {
        AbilitySystemComponent->SetActivationGate(this, MakeShared<FGamePlatformAIActivationGate>(*this,
            ResourceRequestGeneration, GetCurrentGeneration()));
    }
}

void AGamePlatformAIController::HandleAbilityAvatarBindingChanged(const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot)
{ (void)Snapshot; RefreshAIActivationGate(); }

void AGamePlatformAIController::ClearAIActivationGate(bool bDetachAvatarBinding)
{
    if (!AbilitySystemComponent) { return; }
    AbilitySystemComponent->ClearActivationGate(this);
    if (bDetachAvatarBinding)
    {
        AbilitySystemComponent->OnAvatarBindingChanged().Remove(AvatarBindingChangedHandle);
        AvatarBindingChangedHandle.Reset();
        if (bOwnsAbilityActorInfo && AbilitySystemComponent->GetAvatarActor() == GetPawn() &&
            AbilitySystemComponent->GetOwnerActor() == GetPawn())
        { AbilitySystemComponent->ClearAbilityAvatar(); }
        bOwnsAbilityActorInfo = false;
    }
}
