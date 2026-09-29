#include "Server/GamePlatformServerAdmissionSubsystem.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr int32 MaxAdmissionTextChars = 1024;
constexpr int32 MinCredentialBytes = 32;
constexpr int32 MaxCredentialBytes = 16 * 1024;

bool IsBoundedAdmissionText(const FString& Value)
{
    return !Value.IsEmpty() && Value.Len() <= MaxAdmissionTextChars &&
        !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n"));
}
}

bool FGamePlatformServerAdmissionTarget::IsValid() const
{
    return IsBoundedAdmissionText(GameServerId) &&
        IsBoundedAdmissionText(ServerBootId) &&
        IsBoundedAdmissionText(WorldId) &&
        IsBoundedAdmissionText(ExperienceId) &&
        IsBoundedAdmissionText(ProtocolVersion) &&
        ServerStartGeneration > 0;
}

FGamePlatformServerAdmissionProof::~FGamePlatformServerAdmissionProof()
{
    ResetSensitive();
}

FGamePlatformServerAdmissionProof::FGamePlatformServerAdmissionProof(
    FGamePlatformServerAdmissionProof&& Other) noexcept
    : OperationId(Other.OperationId)
    , ReservationId(MoveTemp(Other.ReservationId))
    , AttemptId(MoveTemp(Other.AttemptId))
    , Credential(MoveTemp(Other.Credential))
{
    Other.OperationId.Invalidate();
}

FGamePlatformServerAdmissionProof& FGamePlatformServerAdmissionProof::operator=(
    FGamePlatformServerAdmissionProof&& Other) noexcept
{
    if (this != &Other)
    {
        ResetSensitive();
        OperationId = Other.OperationId;
        ReservationId = MoveTemp(Other.ReservationId);
        AttemptId = MoveTemp(Other.AttemptId);
        Credential = MoveTemp(Other.Credential);
        Other.OperationId.Invalidate();
    }
    return *this;
}

bool FGamePlatformServerAdmissionProof::IsValid() const
{
    return OperationId.IsValid() &&
        IsBoundedAdmissionText(ReservationId) &&
        IsBoundedAdmissionText(AttemptId) &&
        Credential.Num() >= MinCredentialBytes &&
        Credential.Num() <= MaxCredentialBytes;
}

void FGamePlatformServerAdmissionProof::ResetSensitive()
{
    if (!Credential.IsEmpty())
    {
        FMemory::Memzero(Credential.GetData(), Credential.Num());
        Credential.Reset();
    }
}

bool FGamePlatformServerVerifiedAdmission::IsStructurallyValid() const
{
    return AdmissionId.IsValid() && ConnectionId.IsValid() &&
        IsBoundedAdmissionText(PlayerId) && IsBoundedAdmissionText(SessionId) &&
        IsBoundedAdmissionText(GameSessionId) &&
        IsBoundedAdmissionText(AssignmentId) && IsBoundedAdmissionText(ReservationId) &&
        IsBoundedAdmissionText(ServerInstanceId) && IsBoundedAdmissionText(ServerBootId) &&
        IsBoundedAdmissionText(WorldId) && IsBoundedAdmissionText(ExperienceId) &&
        IsBoundedAdmissionText(ProtocolVersion) && ConnectionGeneration > 0 &&
        SessionEpoch > 0 && AuthorityUntil > FDateTime::UtcNow();
}

bool FGamePlatformServerVerifiedAdmission::MatchesTarget(
    const FGamePlatformServerAdmissionTarget& Target) const
{
    return Target.IsValid() && IsStructurallyValid() &&
        ServerInstanceId == Target.GameServerId &&
        ServerBootId == Target.ServerBootId &&
        WorldId == Target.WorldId &&
        ExperienceId == Target.ExperienceId &&
        ProtocolVersion == Target.ProtocolVersion;
}

FName IGamePlatformServerAdmissionProvider::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformServerAdmissionProvider"));
}

bool UGamePlatformServerAdmissionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return IsRunningDedicatedServer() && !IsRunningCommandlet() &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformServerAdmissionSubsystem::Deinitialize()
{
    if (IGamePlatformServerAdmissionProvider* Provider = ResolveUniqueProvider())
    {
        for (const TPair<FGuid, FPendingAdmission>& Pair : PendingAdmissions)
        {
            Provider->CancelOperation(Pair.Key);
        }
    }
    ++OperationGeneration;
    PendingAdmissions.Reset();
    VerifiedAdmissions.Reset();
    ActiveTarget = {};
    AdmissionChanged.Clear();
    Super::Deinitialize();
}

bool UGamePlatformServerAdmissionSubsystem::ConfigureTarget(
    const FGamePlatformServerAdmissionTarget& Target)
{
    check(IsInGameThread());
    if (!Target.IsValid() || !PendingAdmissions.IsEmpty() || !VerifiedAdmissions.IsEmpty())
    {
        return false;
    }
    ActiveTarget = Target;
    return true;
}

bool UGamePlatformServerAdmissionSubsystem::BeginAdmission(
    APlayerController& Controller,
    FGamePlatformServerAdmissionProof Proof,
    FGamePlatformServerAdmissionCompletion Completion)
{
    check(IsInGameThread());
    if (!ActiveTarget.IsValid() || !Proof.IsValid() ||
        Controller.GetWorld() == nullptr || Controller.GetGameInstance() != GetGameInstance())
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionRequestInvalid")));
        }
        return false;
    }
    if (PendingAdmissions.Contains(Proof.OperationId))
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionOperationConflict")));
        }
        return false;
    }
    if (VerifiedAdmissions.Contains(&Controller))
    {
        if (Completion)
        {
            FGamePlatformServerAdmissionResult Result;
            Result.bSucceeded = true;
            Result.Admission = VerifiedAdmissions.FindChecked(&Controller);
            Completion(MoveTemp(Result));
        }
        return true;
    }
    for (const TPair<FGuid, FPendingAdmission>& Pair : PendingAdmissions)
    {
        if (Pair.Value.Controller.Get() == &Controller)
        {
            if (Completion)
            {
                Completion(MakeFailure(TEXT("ServerAdmissionBusy")));
            }
            return false;
        }
    }

    IGamePlatformServerAdmissionProvider* Provider = ResolveUniqueProvider();
    if (!Provider)
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionProviderUnavailable")));
        }
        return false;
    }

    if (NextConnectionGeneration == MAX_uint64)
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionGenerationExhausted")));
        }
        return false;
    }

    const FGuid OperationId = Proof.OperationId;
    const FGuid ConnectionId = FGuid::NewGuid();
    const uint64 ConnectionGeneration = ++NextConnectionGeneration;
    const uint64 Generation = ++OperationGeneration;

    FPendingAdmission Pending;
    Pending.OperationId = OperationId;
    Pending.ConnectionId = ConnectionId;
    Pending.Generation = Generation;
    Pending.ConnectionGeneration = ConnectionGeneration;
    Pending.Controller = &Controller;
    Pending.Completion = MoveTemp(Completion);
    PendingAdmissions.Add(OperationId, MoveTemp(Pending));

    const TWeakObjectPtr<UGamePlatformServerAdmissionSubsystem> WeakThis(this);
    Provider->ValidateAdmission(
        ActiveTarget,
        ConnectionId,
        ConnectionGeneration,
        MoveTemp(Proof),
        [WeakThis, OperationId, Generation](FGamePlatformServerAdmissionResult Result) mutable
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, OperationId, Generation, Result = MoveTemp(Result)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->CompleteAdmission(OperationId, Generation, MoveTemp(Result));
                    }
                });
        });
    return true;
}

bool UGamePlatformServerAdmissionSubsystem::ReleaseAdmission(
    APlayerController& Controller,
    FGamePlatformServerAdmissionCompletion Completion)
{
    check(IsInGameThread());
    FGamePlatformServerVerifiedAdmission Admission;
    if (!VerifiedAdmissions.RemoveAndCopyValue(&Controller, Admission))
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionNotFound")));
        }
        return false;
    }

    AdmissionChanged.Broadcast(&Controller, Admission, false);
    IGamePlatformServerAdmissionProvider* Provider = ResolveUniqueProvider();
    if (!Provider)
    {
        if (Completion)
        {
            Completion(MakeFailure(TEXT("ServerAdmissionProviderUnavailable")));
        }
        return false;
    }

    Provider->ReleaseAdmission(Admission, MoveTemp(Completion));
    return true;
}

bool UGamePlatformServerAdmissionSubsystem::GetVerifiedAdmission(
    const APlayerController& Controller,
    FGamePlatformServerVerifiedAdmission& OutAdmission) const
{
    check(IsInGameThread());
    const FGamePlatformServerVerifiedAdmission* Found = VerifiedAdmissions.Find(
        TWeakObjectPtr<APlayerController>(const_cast<APlayerController*>(&Controller)));
    if (!Found || !Found->MatchesTarget(ActiveTarget))
    {
        return false;
    }
    OutAdmission = *Found;
    return true;
}

IGamePlatformServerAdmissionProvider*
UGamePlatformServerAdmissionSubsystem::ResolveUniqueProvider() const
{
    IModularFeatures& Features = IModularFeatures::Get();
    const TArray<IGamePlatformServerAdmissionProvider*> Providers =
        Features.GetModularFeatureImplementations<IGamePlatformServerAdmissionProvider>(
            IGamePlatformServerAdmissionProvider::GetModularFeatureName());
    return Providers.Num() == 1 ? Providers[0] : nullptr;
}

void UGamePlatformServerAdmissionSubsystem::CompleteAdmission(
    FGuid OperationId,
    uint64 Generation,
    FGamePlatformServerAdmissionResult Result)
{
    check(IsInGameThread());
    FPendingAdmission Pending;
    if (!PendingAdmissions.RemoveAndCopyValue(OperationId, Pending) ||
        Pending.Generation != Generation || !Pending.Controller.IsValid())
    {
        return;
    }

    APlayerController* Controller = Pending.Controller.Get();
    if (!Result.bSucceeded ||
        !Result.Admission.MatchesTarget(ActiveTarget) ||
        Result.Admission.ConnectionId != Pending.ConnectionId ||
        Result.Admission.ConnectionGeneration != Pending.ConnectionGeneration)
    {
        if (Result.ErrorCode.IsNone())
        {
            Result = MakeFailure(TEXT("ServerAdmissionVerificationFailed"));
        }
        if (Pending.Completion)
        {
            Pending.Completion(MoveTemp(Result));
        }
        return;
    }

    if (VerifiedAdmissions.Contains(Controller))
    {
        if (Pending.Completion)
        {
            Pending.Completion(MakeFailure(TEXT("ServerAdmissionConflict")));
        }
        return;
    }

    VerifiedAdmissions.Add(Controller, Result.Admission);
    AdmissionChanged.Broadcast(Controller, Result.Admission, true);
    if (Pending.Completion)
    {
        Pending.Completion(MoveTemp(Result));
    }
}

FGamePlatformServerAdmissionResult
UGamePlatformServerAdmissionSubsystem::MakeFailure(FName ErrorCode)
{
    FGamePlatformServerAdmissionResult Result;
    Result.bSucceeded = false;
    Result.ErrorCode = ErrorCode;
    return Result;
}
