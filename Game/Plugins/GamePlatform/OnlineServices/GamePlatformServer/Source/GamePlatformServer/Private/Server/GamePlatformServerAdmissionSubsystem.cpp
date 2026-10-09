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
        (MatchId.IsEmpty() || IsBoundedAdmissionText(MatchId)) &&
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
    bIsClosing = true;
    ResetTarget(TEXT("ServerAdmissionScopeClosed"));
    AdmissionChanged.Clear();
    Super::Deinitialize();
}

bool UGamePlatformServerAdmissionSubsystem::ConfigureTarget(
    const FGamePlatformServerAdmissionTarget& Target)
{
    check(IsInGameThread());
    if (bIsClosing || bResettingTarget || bStoppingAdmissions || (!bAcceptingAdmissions && ActiveTarget.IsValid()) ||
        !Target.IsValid() || !PendingAdmissions.IsEmpty() || !VerifiedAdmissions.IsEmpty())
    {
        return false;
    }
    ActiveTarget = Target;
    bAcceptingAdmissions = true;
    return true;
}

void UGamePlatformServerAdmissionSubsystem::ResetTarget(FName Reason)
{
    check(IsInGameThread());
    if (bResettingTarget) return;
    TGuardValue<bool> ResetGuard(bResettingTarget, true);
    bAcceptingAdmissions = false;
    ActiveTarget = {};
    // 先移走账本，使同步取消、撤销通知和迟到完成均不能观察或改写正在迭代的旧状态。
    auto Verified = MoveTemp(VerifiedAdmissions);
    VerifiedAdmissions.Reset();
    CancelPendingAdmissions(Reason);
    IGamePlatformServerAdmissionProvider* Provider = ResolveUniqueProvider();
    for (const auto& Pair : Verified)
    {
        if (Pair.Key.IsValid()) AdmissionChanged.Broadcast(Pair.Key.Get(), Pair.Value, false);
        if (Provider) Provider->ReleaseAdmission(Pair.Value, {});
    }
}

void UGamePlatformServerAdmissionSubsystem::StopAcceptingAdmissions(FName Reason)
{
    check(IsInGameThread());
    if (bResettingTarget || bStoppingAdmissions) return;
    // 停止新握手与完整撤销是不同所有权操作；取消通知内的世界退出必须能升级为ResetTarget。
    // 单独栅栏只拒绝重开/重复停止，不能吞掉关停期间对已有连接的完整撤销。
    TGuardValue<bool> StopGuard(bStoppingAdmissions, true);
    bAcceptingAdmissions = false;
    CancelPendingAdmissions(Reason);
}

void UGamePlatformServerAdmissionSubsystem::CancelPendingAdmissions(FName Reason)
{
    auto Pending = MoveTemp(PendingAdmissions);
    PendingAdmissions.Reset();
    IGamePlatformServerAdmissionProvider* Provider = ResolveUniqueProvider();
    for (auto& Pair : Pending)
    {
        if (Provider) Provider->CancelOperation(Pair.Key);
        if (Pair.Value.Completion) Pair.Value.Completion(MakeFailure(
            Reason.IsNone() ? FName(TEXT("ServerAdmissionTargetClosed")) : Reason));
    }
}

bool UGamePlatformServerAdmissionSubsystem::BeginAdmission(
    APlayerController& Controller,
    FGamePlatformServerAdmissionProof Proof,
    FGamePlatformServerAdmissionCompletion Completion)
{
    check(IsInGameThread());
    if (bIsClosing || bResettingTarget || bStoppingAdmissions || !bAcceptingAdmissions || !ActiveTarget.IsValid() || !Proof.IsValid() ||
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
        FGamePlatformServerVerifiedAdmission Current;
        if (!GetVerifiedAdmission(Controller, Current))
        {
            ReleaseAdmission(Controller, {});
            if (Completion) Completion(MakeFailure(TEXT("ServerAdmissionExpired")));
            return false;
        }
        if (Completion)
        {
            FGamePlatformServerAdmissionResult Result;
            Result.bSucceeded = true;
            Result.Admission = MoveTemp(Current);
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
    const auto* Existing = PendingAdmissions.Find(OperationId);
    // 先核对代次再取走账本；旧回调不能移除调用者重试复用OperationId后的新请求。
    if (!Existing || Existing->Generation != Generation) return;
    FPendingAdmission Pending;
    if (!PendingAdmissions.RemoveAndCopyValue(OperationId, Pending))
    {
        return;
    }
    if (!Pending.Controller.IsValid())
    {
        if (Pending.Completion) Pending.Completion(MakeFailure(TEXT("ServerAdmissionConnectionExpired")));
        return;
    }

    APlayerController* Controller = Pending.Controller.Get();
    if (!Result.bSucceeded ||
        !Result.Admission.MatchesTarget(ActiveTarget) ||
        Result.Admission.ConnectionId != Pending.ConnectionId ||
        Result.Admission.ConnectionGeneration != Pending.ConnectionGeneration)
    {
        Result = MakeFailure(Result.ErrorCode.IsNone() ? FName(TEXT("ServerAdmissionVerificationFailed")) : Result.ErrorCode);
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
    // 通知允许项目层同步撤销目标；成功完成必须以广播后的当前连接账本为准。
    const auto* Current = VerifiedAdmissions.Find(Controller);
    if (!Current || Current->AdmissionId != Result.Admission.AdmissionId || !Current->MatchesTarget(ActiveTarget))
        Result = MakeFailure(TEXT("ServerAdmissionRevokedDuringNotification"));
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
