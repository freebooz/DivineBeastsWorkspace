#include "Framework/GamePlatformGameModeBase.h"

#include "Components/GamePlatformExperienceComponent.h"
#include "Definitions/GamePlatformExperienceDefinition.h"
#include "Definitions/GamePlatformPawnDefinition.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "GamePlatformGameplay.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Settings/GamePlatformGameplaySettings.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformTime.h"
#include "Policies/SpawnCandidateOperation.h"

namespace
{
FGamePlatformResult GameplayFailure(FName Code, const TCHAR* Message = TEXT("当前玩法权威条件不满足。"))
{
    return FGamePlatformResult::Failure(Code, Message);
}

struct FAdmissionAuthorityRecord
{
    FGamePlatformGameplayRegistration Registration;
    TWeakObjectPtr<UObject> Owner;
    TSharedPtr<IGamePlatformGameplayAdmissionAuthority> Authority;
};

struct FSpawnPolicyRecord
{
    FGamePlatformGameplayRegistration Registration;
    TWeakObjectPtr<UObject> Owner;
    TSharedPtr<IGamePlatformSpawnPolicy> Policy;
};

struct FPlayerRuntimeRecord
{
    TWeakObjectPtr<APlayerController> Controller;
    FGamePlatformVerifiedPlayerContext Admission;
    FPrimaryAssetId PawnDefinitionId;
    FGamePlatformId PawnLogicalId;
    int64 PlayerGeneration = 0;
    int64 SpawnGeneration = 0;
    int64 PawnGeneration = 0;
    EGamePlatformPlayerStage Stage = EGamePlatformPlayerStage::Unregistered;
    double DeadlineSeconds = 0.0;
    FName ReservedCandidateId;
    TWeakObjectPtr<AActor> ReservedSource;
};

bool SameAdmissionIdentity(const FGamePlatformVerifiedPlayerContext& A, const FGamePlatformVerifiedPlayerContext& B)
{
    return A.AdmissionId == B.AdmissionId
        && A.ConnectionGeneration == B.ConnectionGeneration
        && A.SessionEpoch == B.SessionEpoch;
}

bool SameAdmissionPayload(const FGamePlatformVerifiedPlayerContext& A, const FGamePlatformVerifiedPlayerContext& B)
{
    return SameAdmissionIdentity(A, B)
        && A.ParticipantId == B.ParticipantId
        && A.AssignmentId == B.AssignmentId
        && A.ServerInstanceId == B.ServerInstanceId
        && A.ServerStartGeneration == B.ServerStartGeneration
        && A.ExperienceId == B.ExperienceId
        && A.PawnDefinitionId == B.PawnDefinitionId;
}

bool IsCandidateRegionAllowed(UWorld& World, const FGamePlatformSpawnCandidate& Candidate)
{
    if (!Candidate.RegionId.IsValid())
    {
        return true;
    }

    IGamePlatformWorldService* WorldService = IGamePlatformWorldService::Get(World);
    if (!WorldService)
    {
        return false;
    }

    FGamePlatformId ActualRegion;
    const FGamePlatformResult QueryResult = WorldService->QueryRegion(Candidate.Transform.GetLocation(), ActualRegion);
    return QueryResult.IsSuccess() && ActualRegion == Candidate.RegionId;
}
}

struct FGamePlatformGameplayServerRuntime
{
    FGuid Scope = FGuid::NewGuid();
    FAdmissionAuthorityRecord AdmissionAuthority;
    TMap<FName, FSpawnPolicyRecord> SpawnPolicies;
    TMap<TWeakObjectPtr<APlayerController>, FPlayerRuntimeRecord> Players;
    TMap<FName, TWeakObjectPtr<APlayerController>> CandidateReservations;
    int64 NextPlayerGeneration = 1;
    bool bInternalSpawn = false;
    bool bExternalCall = false;
    bool bClosing = false;
};

AGamePlatformGameModeBase::AGamePlatformGameModeBase()
    : Runtime(MakeUnique<FGamePlatformGameplayServerRuntime>())
{
    GameStateClass = AGamePlatformGameStateBase::StaticClass();
    PlayerControllerClass = AGamePlatformPlayerControllerBase::StaticClass();
    PlayerStateClass = AGamePlatformPlayerStateBase::StaticClass();

    // 默认UE自动出生必须关闭；只有本类在可信准入与体验门禁通过后才能进入内部Spawn作用域。
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;
}

AGamePlatformGameModeBase::AGamePlatformGameModeBase(FVTableHelper& Helper)
    : Super(Helper)
    , Runtime(MakeUnique<FGamePlatformGameplayServerRuntime>())
{
}

AGamePlatformGameModeBase::~AGamePlatformGameModeBase() = default;

void AGamePlatformGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    if (!Runtime)
    {
        Runtime = MakeUnique<FGamePlatformGameplayServerRuntime>();
    }
}

void AGamePlatformGameModeBase::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Runtime && !Runtime->bClosing)
    {
        DrainPlayers(TEXT("WorldEnded"));
        Runtime->bClosing = true;
        Runtime->AdmissionAuthority = {};
        Runtime->SpawnPolicies.Reset();
        Runtime->CandidateReservations.Reset();
    }
    Super::EndPlay(Reason);
}

FGamePlatformGameplayRegistration AGamePlatformGameModeBase::RegisterAdmissionAuthority(
    TWeakObjectPtr<UObject> InOwner,
    TSharedRef<IGamePlatformGameplayAdmissionAuthority> Authority,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || !InOwner.IsValid()
        || InOwner->GetWorld() != GetWorld() || Runtime->AdmissionAuthority.Registration.IsValid())
    {
        OutResult = GameplayFailure(TEXT("AdmissionAuthorityRegistrationRejected"));
        return {};
    }

    FGamePlatformGameplayRegistration Registration{Runtime->Scope, FGuid::NewGuid()};
    Runtime->AdmissionAuthority.Registration = Registration;
    Runtime->AdmissionAuthority.Owner = InOwner;
    Runtime->AdmissionAuthority.Authority = Authority;
    OutResult = FGamePlatformResult::Success();
    return Registration;
}

bool AGamePlatformGameModeBase::UnregisterAdmissionAuthority(const FGamePlatformGameplayRegistration& Registration)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bExternalCall || Registration.ScopeId != Runtime->Scope
        || Runtime->AdmissionAuthority.Registration.RegistrationId != Registration.RegistrationId)
    {
        return false;
    }

    Runtime->AdmissionAuthority = {};
    DrainPlayers(TEXT("AdmissionAuthorityRevoked"));
    return true;
}

FGamePlatformGameplayRegistration AGamePlatformGameModeBase::RegisterSpawnPolicy(
    FName PolicyId,
    TWeakObjectPtr<UObject> InOwner,
    TSharedRef<IGamePlatformSpawnPolicy> Policy,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    const UGamePlatformGameplaySettings* Settings = GetDefault<UGamePlatformGameplaySettings>();
    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || PolicyId.IsNone()
        || PolicyId == FName(TEXT("PlayerStart")) || !InOwner.IsValid() || InOwner->GetWorld() != GetWorld()
        || Runtime->SpawnPolicies.Contains(PolicyId)
        || Runtime->SpawnPolicies.Num() >= Settings->MaximumSpawnPolicies)
    {
        OutResult = GameplayFailure(TEXT("SpawnPolicyRegistrationRejected"));
        return {};
    }

    FGamePlatformGameplayRegistration Registration{Runtime->Scope, FGuid::NewGuid()};
    FSpawnPolicyRecord Record;
    Record.Registration = Registration;
    Record.Owner = InOwner;
    Record.Policy = Policy;
    Runtime->SpawnPolicies.Add(PolicyId, MoveTemp(Record));
    OutResult = FGamePlatformResult::Success();
    return Registration;
}

bool AGamePlatformGameModeBase::UnregisterSpawnPolicy(const FGamePlatformGameplayRegistration& Registration)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bExternalCall || Registration.ScopeId != Runtime->Scope)
    {
        return false;
    }

    FName RemovedPolicy = NAME_None;
    for (auto It = Runtime->SpawnPolicies.CreateIterator(); It; ++It)
    {
        if (It.Value().Registration.RegistrationId == Registration.RegistrationId)
        {
            RemovedPolicy = It.Key();
            It.RemoveCurrent();
            break;
        }
    }

    if (RemovedPolicy.IsNone())
    {
        return false;
    }

    if (const UGamePlatformExperienceComponent* Experience = GetExperienceComponent())
    {
        if (const UGamePlatformExperienceDefinition* Definition = Experience->GetLoadedExperience();
            Definition && Definition->SpawnPolicyId == RemovedPolicy)
        {
            DrainPlayers(TEXT("SpawnPolicyRevoked"));
        }
    }
    return true;
}

FGamePlatformResult AGamePlatformGameModeBase::ValidateAdmission(const APlayerController& Controller, bool bForActivation) const
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || Controller.GetWorld() != GetWorld())
    {
        return GameplayFailure(TEXT("AdmissionScopeInvalid"));
    }

    const FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(const_cast<APlayerController*>(&Controller)));
    if (!Record || !Record->Admission.IsStructurallyValid())
    {
        return GameplayFailure(TEXT("AdmissionMissing"));
    }

    const FAdmissionAuthorityRecord& AuthorityRecord = Runtime->AdmissionAuthority;
    if (!AuthorityRecord.Registration.IsValid() || !AuthorityRecord.Owner.IsValid()
        || AuthorityRecord.Owner->GetWorld() != GetWorld() || !AuthorityRecord.Authority.IsValid())
    {
        return GameplayFailure(TEXT("AdmissionAuthorityUnavailable"));
    }

    TGuardValue<bool> ExternalGuard(Runtime->bExternalCall, true);
    const FGamePlatformResult AuthorityResult = bForActivation
        ? AuthorityRecord.Authority->ValidatePlayerActivation(Controller, Record->Admission)
        : AuthorityRecord.Authority->ValidateCurrentAdmission(Controller, Record->Admission);
    if (!AuthorityResult.IsSuccess())
    {
        return AuthorityResult;
    }

    if (IGamePlatformWorldService* WorldService = IGamePlatformWorldService::Get(*GetWorld()))
    {
        const FGamePlatformWorldReadinessSnapshot World = WorldService->GetReadiness();
        if (!World.Context.ServerInstanceId.IsEmpty() && Record->Admission.ServerInstanceId != World.Context.ServerInstanceId)
        {
            return GameplayFailure(TEXT("AdmissionServerInstanceMismatch"));
        }
        if (World.Context.ServerStartGeneration > 0 && Record->Admission.ServerStartGeneration != World.Context.ServerStartGeneration)
        {
            return GameplayFailure(TEXT("AdmissionServerGenerationMismatch"));
        }
    }

    const UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    if (Experience)
    {
        const FGamePlatformExperienceSnapshot Snapshot = Experience->GetExperienceSnapshot();
        if (Snapshot.IsAssigned() && Snapshot.ExperienceId != Record->Admission.ExperienceId)
        {
            return GameplayFailure(TEXT("AdmissionExperienceMismatch"));
        }
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult AGamePlatformGameModeBase::SubmitVerifiedAdmission(
    APlayerController& Controller,
    const FGamePlatformVerifiedPlayerContext& Context)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || !HasAuthority()
        || Controller.GetWorld() != GetWorld() || !Context.IsStructurallyValid())
    {
        return GameplayFailure(TEXT("VerifiedAdmissionRejected"));
    }

    if (!Runtime->AdmissionAuthority.Registration.IsValid() || !Runtime->AdmissionAuthority.Owner.IsValid()
        || Runtime->AdmissionAuthority.Owner->GetWorld() != GetWorld() || !Runtime->AdmissionAuthority.Authority.IsValid())
    {
        return GameplayFailure(TEXT("AdmissionAuthorityUnavailable"));
    }

    {
        TGuardValue<bool> ExternalGuard(Runtime->bExternalCall, true);
        const FGamePlatformResult AuthorityResult =
            Runtime->AdmissionAuthority.Authority->ValidateCurrentAdmission(Controller, Context);
        if (!AuthorityResult.IsSuccess())
        {
            return AuthorityResult;
        }
    }

    const TWeakObjectPtr<APlayerController> Key(&Controller);
    if (FPlayerRuntimeRecord* Existing = Runtime->Players.Find(Key))
    {
        if (SameAdmissionPayload(Existing->Admission, Context))
        {
            return FGamePlatformResult::Success();
        }
        return GameplayFailure(TEXT("AdmissionConflict"));
    }

    if (const UGamePlatformExperienceComponent* Experience = GetExperienceComponent())
    {
        const FGamePlatformExperienceSnapshot Snapshot = Experience->GetExperienceSnapshot();
        if (Snapshot.IsAssigned() && Snapshot.ExperienceId != Context.ExperienceId)
        {
            return GameplayFailure(TEXT("AdmissionExperienceMismatch"));
        }
    }

    if (IGamePlatformWorldService* WorldService = IGamePlatformWorldService::Get(*GetWorld()))
    {
        const FGamePlatformWorldReadinessSnapshot World = WorldService->GetReadiness();
        if (!World.Context.ServerInstanceId.IsEmpty() && Context.ServerInstanceId != World.Context.ServerInstanceId)
        {
            return GameplayFailure(TEXT("AdmissionServerInstanceMismatch"));
        }
        if (World.Context.ServerStartGeneration > 0 && Context.ServerStartGeneration != World.Context.ServerStartGeneration)
        {
            return GameplayFailure(TEXT("AdmissionServerGenerationMismatch"));
        }
    }

    FPlayerRuntimeRecord Record;
    Record.Controller = &Controller;
    Record.Admission = Context;
    Record.PlayerGeneration = Runtime->NextPlayerGeneration++;
    Record.Stage = EGamePlatformPlayerStage::Accepted;

    Runtime->Players.Add(Key, MoveTemp(Record));
    PublishPlayer(Controller, EGamePlatformPlayerStage::Accepted);
    AdvancePlayer(Controller);
    return FGamePlatformResult::Success();
}

FGamePlatformResult AGamePlatformGameModeBase::RevokeVerifiedAdmission(
    APlayerController& Controller,
    const FGamePlatformAdmissionRevocation& Revocation)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bExternalCall || Controller.GetWorld() != GetWorld() || !Revocation.IsStructurallyValid())
    {
        return GameplayFailure(TEXT("AdmissionRevocationRejected"));
    }

    const FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    if (!Record || Record->Admission.AdmissionId != Revocation.AdmissionId
        || Record->Admission.ConnectionGeneration != Revocation.ConnectionGeneration
        || Record->Admission.SessionEpoch != Revocation.SessionEpoch)
    {
        return GameplayFailure(TEXT("AdmissionRevocationStale"));
    }

    RemovePlayer(Controller, TEXT("AdmissionRevoked"), false);
    return FGamePlatformResult::Success();
}

FGamePlatformSpawnEligibility AGamePlatformGameModeBase::EvaluatePlayerStartEligibility(APlayerController& Controller) const
{
    FGamePlatformSpawnEligibility Result;
    Result.State = EGamePlatformSpawnEligibility::Denied;
    Result.ReasonCode = TEXT("AdmissionMissing");

    if (!Runtime || Runtime->bClosing || Controller.GetWorld() != GetWorld())
    {
        Result.ReasonCode = TEXT("GameplayScopeInvalid");
        return Result;
    }

    const FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    if (!Record)
    {
        return Result;
    }

    const FGamePlatformResult Admission = ValidateAdmission(Controller);
    if (!Admission.IsSuccess())
    {
        Result.ReasonCode = Admission.Code.IsNone() ? FName(TEXT("AdmissionInvalid")) : Admission.Code;
        return Result;
    }

    const UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    if (!Experience)
    {
        Result.State = EGamePlatformSpawnEligibility::Waiting;
        Result.ReasonCode = TEXT("ExperienceUnavailable");
        Result.RetryAfterSeconds = 0.05f;
        return Result;
    }

    const FGamePlatformExperienceSnapshot Snapshot = Experience->GetExperienceSnapshot();
    if (!Snapshot.IsServerActive())
    {
        if (Snapshot.Stage == EGamePlatformExperienceStage::Failed
            || Snapshot.Stage == EGamePlatformExperienceStage::Draining
            || Snapshot.Stage == EGamePlatformExperienceStage::Released)
        {
            Result.ReasonCode = TEXT("ExperienceNotActive");
            return Result;
        }

        Result.State = EGamePlatformSpawnEligibility::Waiting;
        Result.ReasonCode = TEXT("ExperiencePreparing");
        Result.RetryAfterSeconds = 0.05f;
        return Result;
    }

    if (Snapshot.ExperienceId != Record->Admission.ExperienceId)
    {
        Result.ReasonCode = TEXT("AdmissionExperienceMismatch");
        return Result;
    }

    const UGamePlatformExperienceDefinition* Definition = Experience->GetLoadedExperience();
    const UGamePlatformPawnDefinition* PawnDefinition = Experience->GetLoadedDefaultPawn();
    if (!Definition || !PawnDefinition || !PawnDefinition->PawnClass.Get())
    {
        Result.State = EGamePlatformSpawnEligibility::Waiting;
        Result.ReasonCode = TEXT("PawnDefinitionPreparing");
        Result.RetryAfterSeconds = 0.05f;
        return Result;
    }

    const FPrimaryAssetId RequestedPawn = Record->Admission.PawnDefinitionId.IsValid()
        ? Record->Admission.PawnDefinitionId
        : Definition->DefaultPawnDefinitionId;
    if (RequestedPawn != Definition->DefaultPawnDefinitionId)
    {
        Result.ReasonCode = TEXT("PerPlayerPawnOverrideUnsupported");
        return Result;
    }

    if (IGamePlatformWorldService* WorldService = IGamePlatformWorldService::Get(*GetWorld()))
    {
        const FGamePlatformWorldReadinessSnapshot World = WorldService->GetReadiness();
        if (World.Context.ContextGeneration != Snapshot.WorldContextGeneration || !World.bWorldNotTearingDown)
        {
            Result.ReasonCode = TEXT("WorldGenerationExpired");
            return Result;
        }
        if (World.Context.ReadinessState != EGamePlatformWorldReadiness::Ready)
        {
            Result.State = EGamePlatformSpawnEligibility::Waiting;
            Result.ReasonCode = TEXT("WorldNotReady");
            Result.RetryAfterSeconds = 0.05f;
            return Result;
        }
    }
    else
    {
        Result.ReasonCode = TEXT("WorldServiceUnavailable");
        return Result;
    }

    Result.State = EGamePlatformSpawnEligibility::Allowed;
    Result.ReasonCode = NAME_None;
    Result.RetryAfterSeconds = 0.f;
    return Result;
}

void AGamePlatformGameModeBase::PublishPlayer(
    APlayerController& Controller,
    EGamePlatformPlayerStage Stage,
    FName Code)
{
    if (!Runtime)
    {
        return;
    }

    FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    AGamePlatformPlayerStateBase* PlayerState = Controller.GetPlayerState<AGamePlatformPlayerStateBase>();
    if (!Record || !PlayerState || !PlayerState->HasAuthority())
    {
        return;
    }

    FGamePlatformPlayerLifecycleSnapshot Snapshot = PlayerState->GetLifecycleSnapshot();
    Snapshot.ParticipantId = Record->Admission.ParticipantId;
    Snapshot.PlayerGeneration = Record->PlayerGeneration;
    Snapshot.SpawnGeneration = Record->SpawnGeneration;
    Snapshot.PawnGeneration = Record->PawnGeneration;
    Snapshot.StateRevision = FMath::Max<int64>(0, Snapshot.StateRevision) + 1;
    Snapshot.Stage = Stage;
    Snapshot.PawnDefinitionId = Record->PawnLogicalId;
    Snapshot.ControlledPawn = Controller.GetPawn();
    Snapshot.StatusCode = Code;

    Record->Stage = Stage;
    PlayerState->Publish(Snapshot);
}

void AGamePlatformGameModeBase::AdvancePlayer(APlayerController& Controller)
{
    if (!Runtime || Runtime->bClosing)
    {
        return;
    }

    FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    if (!Record)
    {
        return;
    }

    const FGamePlatformResult Admission = ValidateAdmission(Controller);
    if (!Admission.IsSuccess())
    {
        RemovePlayer(Controller, Admission.Code.IsNone() ? FName(TEXT("AdmissionInvalid")) : Admission.Code, true);
        return;
    }

    UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    const double Now = FPlatformTime::Seconds();
    const UGamePlatformGameplaySettings* Settings = GetDefault<UGamePlatformGameplaySettings>();

    if (Record->Stage == EGamePlatformPlayerStage::Accepted)
    {
        Record->DeadlineSeconds = Now + Settings->MaximumPhaseTimeoutSeconds;
        PublishPlayer(Controller, EGamePlatformPlayerStage::WaitingExperience);
        Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
        if (!Record)
        {
            return;
        }
    }

    if (Record->Stage == EGamePlatformPlayerStage::WaitingExperience)
    {
        if (!Experience)
        {
            if (Now >= Record->DeadlineSeconds)
            {
                RemovePlayer(Controller, TEXT("ExperienceUnavailable"), true);
            }
            return;
        }

        const FGamePlatformExperienceSnapshot Snapshot = Experience->GetExperienceSnapshot();
        if (Snapshot.Stage == EGamePlatformExperienceStage::Failed
            || Snapshot.Stage == EGamePlatformExperienceStage::Draining
            || Snapshot.Stage == EGamePlatformExperienceStage::Released)
        {
            RemovePlayer(Controller, TEXT("ExperienceNotActive"), true);
            return;
        }
        if (!Snapshot.IsServerActive())
        {
            if (Now >= Record->DeadlineSeconds)
            {
                RemovePlayer(Controller, TEXT("ExperienceWaitTimedOut"), true);
            }
            return;
        }
        if (Snapshot.ExperienceId != Record->Admission.ExperienceId)
        {
            RemovePlayer(Controller, TEXT("AdmissionExperienceMismatch"), true);
            return;
        }

        const UGamePlatformExperienceDefinition* Definition = Experience->GetLoadedExperience();
        const UGamePlatformPawnDefinition* PawnDefinition = Experience->GetLoadedDefaultPawn();
        if (!Definition || !PawnDefinition || !PawnDefinition->PawnClass.Get())
        {
            if (Now >= Record->DeadlineSeconds)
            {
                RemovePlayer(Controller, TEXT("PawnDefinitionWaitTimedOut"), true);
            }
            return;
        }

        Record->PawnDefinitionId = Record->Admission.PawnDefinitionId.IsValid()
            ? Record->Admission.PawnDefinitionId
            : Definition->DefaultPawnDefinitionId;
        if (Record->PawnDefinitionId != Definition->DefaultPawnDefinitionId)
        {
            RemovePlayer(Controller, TEXT("PerPlayerPawnOverrideUnsupported"), true);
            return;
        }
        if (!FGamePlatformId::TryParse(Record->PawnDefinitionId.PrimaryAssetName.ToString(), Record->PawnLogicalId))
        {
            RemovePlayer(Controller, TEXT("PawnDefinitionIdentityInvalid"), true);
            return;
        }

        ++Record->SpawnGeneration;
        Record->DeadlineSeconds = Now + Definition->SpawnTimeoutSeconds;
        PublishPlayer(Controller, EGamePlatformPlayerStage::WaitingSpawn);
        Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
        if (!Record)
        {
            return;
        }
    }

    if (Record->Stage == EGamePlatformPlayerStage::WaitingSpawn)
    {
        const FGamePlatformSpawnEligibility Eligibility = EvaluatePlayerStartEligibility(Controller);
        if (Eligibility.State == EGamePlatformSpawnEligibility::Denied)
        {
            RemovePlayer(Controller,
                Eligibility.ReasonCode.IsNone() ? FName(TEXT("SpawnEligibilityDenied")) : Eligibility.ReasonCode,
                true);
            return;
        }
        if (Eligibility.State == EGamePlatformSpawnEligibility::Waiting)
        {
            if (Now >= Record->DeadlineSeconds)
            {
                RemovePlayer(Controller, TEXT("SpawnWaitTimedOut"), true);
            }
            return;
        }

        TrySpawn(Controller);
        return;
    }

    if (Record->Stage == EGamePlatformPlayerStage::AwaitingClient)
    {
        if (Now >= Record->DeadlineSeconds)
        {
            RemovePlayer(Controller, TEXT("ClientPreparationTimedOut"), true);
        }
        return;
    }

    if (Record->Stage == EGamePlatformPlayerStage::Active)
    {
        if (!Controller.GetPawn())
        {
            RemovePlayer(Controller, TEXT("ControlledPawnLost"), true);
        }
    }
}

void AGamePlatformGameModeBase::TrySpawn(APlayerController& Controller)
{
    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || Runtime->bInternalSpawn)
    {
        return;
    }

    FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    if (!Record || Record->Stage != EGamePlatformPlayerStage::WaitingSpawn || !Experience)
    {
        return;
    }

    const UGamePlatformExperienceDefinition* Definition = Experience->GetLoadedExperience();
    const UGamePlatformPawnDefinition* PawnDefinition = Experience->GetLoadedDefaultPawn();
    const FGamePlatformExperienceSnapshot ExperienceSnapshot = Experience->GetExperienceSnapshot();
    if (!Definition || !PawnDefinition || !ExperienceSnapshot.IsServerActive())
    {
        return;
    }

    FGamePlatformSpawnRequest Request;
    Request.World = GetWorld();
    Request.Controller = &Controller;
    Request.OperationId.WorldContextGeneration = ExperienceSnapshot.WorldContextGeneration;
    Request.OperationId.PlayerGeneration = Record->PlayerGeneration;
    Request.OperationId.SpawnGeneration = Record->SpawnGeneration;
    Request.ExperienceId = ExperienceSnapshot.ExperienceId;
    Request.PawnDefinitionId = Record->PawnDefinitionId;
    Request.SpawnPolicyId = Definition->SpawnPolicyId;

    TArray<FGamePlatformSpawnCandidate> Candidates;
    if (Definition->SpawnPolicyId == FName(TEXT("PlayerStart")))
    {
        int32 Priority = 0;
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* Start = *It;
            if (!Start || Start->GetWorld() != GetWorld())
            {
                continue;
            }

            FGamePlatformSpawnCandidate Candidate;
            Candidate.CandidateId = Start->GetFName();
            Candidate.Priority = Priority++;
            Candidate.Source = Start;
            Candidate.Transform = Start->GetActorTransform();
            Candidates.Add(MoveTemp(Candidate));
        }
    }
    else
    {
        FSpawnPolicyRecord* PolicyRecord = Runtime->SpawnPolicies.Find(Definition->SpawnPolicyId);
        if (!PolicyRecord || !PolicyRecord->Owner.IsValid() || PolicyRecord->Owner->GetWorld() != GetWorld()
            || !PolicyRecord->Policy.IsValid())
        {
            RemovePlayer(Controller, TEXT("SpawnPolicyUnavailable"), true);
            return;
        }

        FGamePlatformResult CollectResult;
        {
            TGuardValue<bool> ExternalGuard(Runtime->bExternalCall, true);
            CollectResult = PolicyRecord->Policy->CollectCandidates(Request, Candidates);
        }
        if (CollectResult.Status == EGamePlatformResultStatus::NotExecuted)
        {
            return;
        }
        if (!CollectResult.IsSuccess())
        {
            RemovePlayer(Controller,
                CollectResult.Code.IsNone() ? FName(TEXT("SpawnPolicyFailed")) : CollectResult.Code,
                true);
            return;
        }
    }

    const UGamePlatformGameplaySettings* Settings = GetDefault<UGamePlatformGameplaySettings>();
    if (Candidates.Num() > Settings->MaximumSpawnCandidates)
    {
        RemovePlayer(Controller, TEXT("TooManySpawnCandidates"), true);
        return;
    }

    TSet<FName> CandidateIds;
    Candidates.RemoveAll([&](const FGamePlatformSpawnCandidate& Candidate)
    {
        if (!Candidate.IsStructurallyValid(*GetWorld()) || CandidateIds.Contains(Candidate.CandidateId)
            || !IsCandidateRegionAllowed(*GetWorld(), Candidate))
        {
            return true;
        }
        CandidateIds.Add(Candidate.CandidateId);

        const TWeakObjectPtr<APlayerController>* ReservedBy = Runtime->CandidateReservations.Find(Candidate.CandidateId);
        return ReservedBy && ReservedBy->IsValid() && ReservedBy->Get() != &Controller;
    });

    Candidates.Sort([](const FGamePlatformSpawnCandidate& A, const FGamePlatformSpawnCandidate& B)
    {
        if (A.Priority != B.Priority)
        {
            return A.Priority < B.Priority;
        }
        return A.CandidateId.ToString() < B.CandidateId.ToString();
    });

    if (Candidates.IsEmpty())
    {
        return;
    }

    PublishPlayer(Controller, EGamePlatformPlayerStage::Spawning);
    Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    if (!Record)
    {
        return;
    }

    for (const FGamePlatformSpawnCandidate& Candidate : Candidates)
    {
        Runtime->CandidateReservations.Add(Candidate.CandidateId, &Controller);
        Record->ReservedCandidateId = Candidate.CandidateId;
        Record->ReservedSource = Candidate.Source;

        {
            TGuardValue<bool> InternalSpawnGuard(Runtime->bInternalSpawn, true);
            // 来源Actor仅提供候选身份；真正生成位置必须与已通过区域验证的Transform相同。
            GamePlatformGameplay::Policy::RestartAtValidatedTransform(Candidate,
                [this, &Controller](const FTransform& Transform) { Super::RestartPlayerAtTransform(&Controller, Transform); });
        }

        Runtime->CandidateReservations.Remove(Candidate.CandidateId);
        Record->ReservedCandidateId = NAME_None;
        Record->ReservedSource.Reset();

        APawn* Pawn = Controller.GetPawn();
        if (!Pawn || Pawn->GetWorld() != GetWorld())
        {
            continue;
        }

        ++Record->PawnGeneration;
        PublishPlayer(Controller, EGamePlatformPlayerStage::Possessed);
        PublishPlayer(Controller, EGamePlatformPlayerStage::AwaitingClient);

        AGamePlatformPlayerControllerBase* PlatformController = Cast<AGamePlatformPlayerControllerBase>(&Controller);
        AGamePlatformPlayerStateBase* PlatformState = Controller.GetPlayerState<AGamePlatformPlayerStateBase>();
        if (!PlatformController || !PlatformState)
        {
            RemovePlayer(Controller, TEXT("GameplayFrameworkClassMismatch"), true);
            return;
        }

        const FGamePlatformPlayerLifecycleSnapshot Lifecycle = PlatformState->GetLifecycleSnapshot();
        FGamePlatformPreparationToken Token;
        Token.TokenId = FGuid::NewGuid();
        Token.WorldContextGeneration = ExperienceSnapshot.WorldContextGeneration;
        Token.ExperienceEpoch = ExperienceSnapshot.ExperienceEpoch;
        Token.PlayerGeneration = Record->PlayerGeneration;
        Token.PawnGeneration = Record->PawnGeneration;
        Token.StateRevision = Lifecycle.StateRevision;
        PlatformController->SetPreparationToken(Token);

        Record->DeadlineSeconds = FPlatformTime::Seconds() + Definition->ClientPreparationTimeoutSeconds;
        return;
    }

    PublishPlayer(Controller, EGamePlatformPlayerStage::WaitingSpawn, TEXT("SpawnAttemptFailed"));
}

FGamePlatformResult AGamePlatformGameModeBase::AcceptPreparation(
    APlayerController& Controller,
    const FGamePlatformPreparationToken& Token)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || Controller.GetWorld() != GetWorld() || !Token.IsValid())
    {
        return GameplayFailure(TEXT("PreparationRejected"));
    }

    FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    AGamePlatformPlayerControllerBase* PlatformController = Cast<AGamePlatformPlayerControllerBase>(&Controller);
    AGamePlatformPlayerStateBase* PlatformState = Controller.GetPlayerState<AGamePlatformPlayerStateBase>();
    UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    if (!Record || !PlatformController || !PlatformState || !Experience
        || Record->Stage != EGamePlatformPlayerStage::AwaitingClient)
    {
        return GameplayFailure(TEXT("PreparationStageMismatch"));
    }

    const FGamePlatformPreparationToken CurrentToken = PlatformController->GetPreparationToken();
    const FGamePlatformPlayerLifecycleSnapshot Lifecycle = PlatformState->GetLifecycleSnapshot();
    const FGamePlatformExperienceSnapshot ExperienceSnapshot = Experience->GetExperienceSnapshot();
    if (!(CurrentToken == Token)
        || Lifecycle.Stage != EGamePlatformPlayerStage::AwaitingClient
        || Lifecycle.ControlledPawn != Controller.GetPawn()
        || Token.WorldContextGeneration != ExperienceSnapshot.WorldContextGeneration
        || Token.ExperienceEpoch != ExperienceSnapshot.ExperienceEpoch
        || Token.PlayerGeneration != Record->PlayerGeneration
        || Token.PawnGeneration != Record->PawnGeneration
        || Token.StateRevision != Lifecycle.StateRevision)
    {
        return GameplayFailure(TEXT("PreparationTokenStale"));
    }

    // 客户端准备事实不能批准缺失的必要Pawn资源；服务器在消费令牌前复核项目激活门禁。
    const FGamePlatformResult Admission = ValidateAdmission(Controller, true);
    if (!Admission.IsSuccess() || !ExperienceSnapshot.IsServerActive())
    {
        return Admission.IsSuccess() ? GameplayFailure(TEXT("ExperienceNotActive")) : Admission;
    }

    PlatformController->SetPreparationToken({});
    PublishPlayer(Controller, EGamePlatformPlayerStage::Active);
    return FGamePlatformResult::Success();
}

bool AGamePlatformGameModeBase::IsPlayerGameplayActive(const APlayerController& Controller) const
{
    if (!Runtime || Runtime->bClosing || Controller.GetWorld() != GetWorld())
    {
        return false;
    }

    const FPlayerRuntimeRecord* Record = Runtime->Players.Find(
        TWeakObjectPtr<APlayerController>(const_cast<APlayerController*>(&Controller)));
    const AGamePlatformPlayerStateBase* State = Controller.GetPlayerState<AGamePlatformPlayerStateBase>();
    if (!Record || !State || Record->Stage != EGamePlatformPlayerStage::Active
        || State->GetLifecycleSnapshot().Stage != EGamePlatformPlayerStage::Active
        || State->GetLifecycleSnapshot().ControlledPawn != Controller.GetPawn()
        || !Controller.GetPawn())
    {
        return false;
    }

    return ValidateAdmission(Controller).IsSuccess();
}

FGamePlatformResult AGamePlatformGameModeBase::RequestServerRestart(APlayerController& Controller)
{
    check(IsInGameThread());

    if (!Runtime || Runtime->bClosing || Runtime->bExternalCall || Controller.GetWorld() != GetWorld())
    {
        return GameplayFailure(TEXT("RestartRejected"));
    }

    FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(&Controller));
    UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    const UGamePlatformExperienceDefinition* Definition = Experience ? Experience->GetLoadedExperience() : nullptr;
    if (!Record || !Definition || Record->Stage != EGamePlatformPlayerStage::Active
        || Definition->RespawnPolicy != EGamePlatformRespawnPolicy::ServerAuthorized)
    {
        return GameplayFailure(TEXT("RestartNotAuthorized"));
    }

    const FGamePlatformResult Admission = ValidateAdmission(Controller);
    if (!Admission.IsSuccess())
    {
        return Admission;
    }

    if (AGamePlatformPlayerControllerBase* PlatformController = Cast<AGamePlatformPlayerControllerBase>(&Controller))
    {
        PlatformController->SetPreparationToken({});
    }

    if (APawn* Pawn = Controller.GetPawn())
    {
        Controller.UnPossess();
        Pawn->Destroy();
    }

    ++Record->SpawnGeneration;
    Record->DeadlineSeconds = FPlatformTime::Seconds() + Definition->SpawnTimeoutSeconds;
    PublishPlayer(Controller, EGamePlatformPlayerStage::WaitingSpawn);
    AdvancePlayer(Controller);
    return FGamePlatformResult::Success();
}

void AGamePlatformGameModeBase::RemovePlayer(
    APlayerController& Controller,
    FName Reason,
    bool bFailed)
{
    if (!Runtime)
    {
        return;
    }

    const TWeakObjectPtr<APlayerController> Key(&Controller);
    FPlayerRuntimeRecord* Record = Runtime->Players.Find(Key);
    if (!Record)
    {
        return;
    }

    if (Record->Stage != EGamePlatformPlayerStage::Leaving
        && Record->Stage != EGamePlatformPlayerStage::Removed
        && Record->Stage != EGamePlatformPlayerStage::Failed)
    {
        PublishPlayer(Controller, EGamePlatformPlayerStage::Leaving, Reason);
        Record = Runtime->Players.Find(Key);
        if (!Record)
        {
            return;
        }
    }

    if (AGamePlatformPlayerControllerBase* PlatformController = Cast<AGamePlatformPlayerControllerBase>(&Controller))
    {
        PlatformController->SetPreparationToken({});
    }

    if (!Record->ReservedCandidateId.IsNone())
    {
        Runtime->CandidateReservations.Remove(Record->ReservedCandidateId);
        Record->ReservedCandidateId = NAME_None;
        Record->ReservedSource.Reset();
    }

    if (APawn* Pawn = Controller.GetPawn())
    {
        Controller.UnPossess();
        Pawn->Destroy();
    }

    PublishPlayer(
        Controller,
        bFailed ? EGamePlatformPlayerStage::Failed : EGamePlatformPlayerStage::Removed,
        Reason);
    Runtime->Players.Remove(Key);
}

void AGamePlatformGameModeBase::DrainPlayers(FName Reason)
{
    if (!Runtime)
    {
        return;
    }

    TArray<TWeakObjectPtr<APlayerController>> Controllers;
    Runtime->Players.GetKeys(Controllers);
    for (const TWeakObjectPtr<APlayerController>& Controller : Controllers)
    {
        if (Controller.IsValid())
        {
            RemovePlayer(*Controller.Get(), Reason, false);
        }
        else
        {
            Runtime->Players.Remove(Controller);
        }
    }
    Runtime->CandidateReservations.Reset();
}

UGamePlatformExperienceComponent* AGamePlatformGameModeBase::GetExperienceComponent() const
{
    const AGamePlatformGameStateBase* State = GetGameState<AGamePlatformGameStateBase>();
    return State ? State->GetExperienceComponent() : nullptr;
}

void AGamePlatformGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Runtime || Runtime->bClosing)
    {
        return;
    }

    if (Runtime->AdmissionAuthority.Registration.IsValid()
        && (!Runtime->AdmissionAuthority.Owner.IsValid()
            || Runtime->AdmissionAuthority.Owner->GetWorld() != GetWorld()
            || !Runtime->AdmissionAuthority.Authority.IsValid()))
    {
        Runtime->AdmissionAuthority = {};
        DrainPlayers(TEXT("AdmissionAuthorityExpired"));
        return;
    }

    for (auto It = Runtime->SpawnPolicies.CreateIterator(); It; ++It)
    {
        if (!It.Value().Owner.IsValid() || It.Value().Owner->GetWorld() != GetWorld() || !It.Value().Policy.IsValid())
        {
            It.RemoveCurrent();
        }
    }

    TArray<TWeakObjectPtr<APlayerController>> Controllers;
    Runtime->Players.GetKeys(Controllers);
    for (const TWeakObjectPtr<APlayerController>& Controller : Controllers)
    {
        if (Controller.IsValid())
        {
            AdvancePlayer(*Controller.Get());
        }
        else
        {
            Runtime->Players.Remove(Controller);
        }
    }
}

void AGamePlatformGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // UE默认入口只作为“已有可信登记”的重算触发器；无准入时故意不调用Super，避免默认自动出生。
    if (NewPlayer && Runtime && Runtime->Players.Contains(TWeakObjectPtr<APlayerController>(NewPlayer)))
    {
        AdvancePlayer(*NewPlayer);
    }
}

bool AGamePlatformGameModeBase::PlayerCanRestart_Implementation(APlayerController* Player)
{
    if (!Player || !Runtime || !Runtime->bInternalSpawn)
    {
        return false;
    }

    const FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(Player));
    return Record && Record->Stage == EGamePlatformPlayerStage::Spawning
        && ValidateAdmission(*Player).IsSuccess();
}

void AGamePlatformGameModeBase::RestartPlayer(AController* NewPlayer)
{
    if (Runtime && Runtime->bInternalSpawn)
    {
        Super::RestartPlayer(NewPlayer);
    }
}

void AGamePlatformGameModeBase::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
    if (Runtime && Runtime->bInternalSpawn)
    {
        Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
    }
}

void AGamePlatformGameModeBase::RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform)
{
    if (Runtime && Runtime->bInternalSpawn)
    {
        Super::RestartPlayerAtTransform(NewPlayer, SpawnTransform);
    }
}

APawn* AGamePlatformGameModeBase::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
    return Runtime && Runtime->bInternalSpawn
        ? Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot)
        : nullptr;
}

APawn* AGamePlatformGameModeBase::SpawnDefaultPawnAtTransform_Implementation(
    AController* NewPlayer,
    const FTransform& SpawnTransform)
{
    return Runtime && Runtime->bInternalSpawn
        ? Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform)
        : nullptr;
}

UClass* AGamePlatformGameModeBase::GetDefaultPawnClassForController_Implementation(AController* Controller)
{
    if (!Runtime || !Runtime->bInternalSpawn)
    {
        return nullptr;
    }

    const UGamePlatformExperienceComponent* Experience = GetExperienceComponent();
    const UGamePlatformPawnDefinition* PawnDefinition = Experience ? Experience->GetLoadedDefaultPawn() : nullptr;
    UClass* PawnClass = PawnDefinition ? PawnDefinition->PawnClass.Get() : nullptr;
    return PawnClass && PawnClass->IsChildOf(APawn::StaticClass()) && !PawnClass->HasAnyClassFlags(CLASS_Abstract)
        ? PawnClass
        : nullptr;
}

AActor* AGamePlatformGameModeBase::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    if (!Runtime || !Runtime->bInternalSpawn)
    {
        // UE InitNewPlayer在准入握手前查询Controller初始位置。这里仅查询地图Actor，不生成Pawn、
        // 不占用玩法候选、不修改Active资格；禁止查询会让引擎直接拒绝Login，握手因此永远无法完成。
        // 忽略外部Portal名称，实际玩法出生仍只消费后续通过准入和候选占位验证的ReservedSource。
        return Super::FindPlayerStart_Implementation(Player, FString());
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(Player))
    {
        if (const FPlayerRuntimeRecord* Record = Runtime->Players.Find(TWeakObjectPtr<APlayerController>(PlayerController)))
        {
            if (Record->ReservedSource.IsValid())
            {
                return Record->ReservedSource.Get();
            }
        }
    }
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void AGamePlatformGameModeBase::Logout(AController* Exiting)
{
    if (APlayerController* PlayerController = Cast<APlayerController>(Exiting))
    {
        RemovePlayer(*PlayerController, TEXT("PlayerLogout"), false);
    }
    Super::Logout(Exiting);
}
