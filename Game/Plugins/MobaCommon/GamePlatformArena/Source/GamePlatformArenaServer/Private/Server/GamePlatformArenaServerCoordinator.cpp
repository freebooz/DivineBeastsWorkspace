#include "Server/GamePlatformArenaServerCoordinator.h"

#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"
#include "TimerManager.h"

namespace
{
constexpr float MaxSettlementRetryDelaySeconds = 32.0f;
}

FGamePlatformArenaServerCoordinator::FGamePlatformArenaServerCoordinator(
    TSharedRef<FGamePlatformArenaServerBackendClient> InBackendClient)
    : BackendClient(MoveTemp(InBackendClient))
{
}

void FGamePlatformArenaServerCoordinator::LoadAndApplyAssignment(
    AGamePlatformArenaGameMode* GameMode,
    TFunction<void(bool, const FString&)> Completion)
{
    if (GameMode == nullptr) { Completion(false, TEXT("GameMode missing.")); return; }
    const TWeakObjectPtr<AGamePlatformArenaGameMode> WeakMode(GameMode);
    BackendClient->GetAssignment(
        [Self = AsShared(), WeakMode, Completion = MoveTemp(Completion)](bool bSuccess, const FGamePlatformArenaAssignment& Assignment, const FString& Error) mutable
        {
            AGamePlatformArenaGameMode* Mode = WeakMode.Get();
            if (!bSuccess || Mode == nullptr) { Completion(false, Error.IsEmpty() ? TEXT("GameMode destroyed.") : Error); return; }
            FString ApplyError;
            if (Self->ProjectExtension != nullptr)
            {
                FGamePlatformArenaModeSpec ProjectModeSpec;
                if (!Self->ProjectExtension->ResolveModeSpec(
                        Assignment,
                        ProjectModeSpec,
                        ApplyError) ||
                    !Self->ProjectExtension->ValidateAndConfigureGameMode(
                        *Mode,
                        Assignment,
                        ProjectModeSpec,
                        ApplyError))
                {
                    Completion(false, ApplyError);
                    return;
                }
                Completion(
                    Mode->ApplyAssignmentWithModeSpec(
                        Assignment,
                        ProjectModeSpec,
                        ApplyError),
                    ApplyError);
                return;
            }
            Completion(Mode->ApplyAssignment(Assignment, ApplyError), ApplyError);
        });
}

void FGamePlatformArenaServerCoordinator::SubmitPendingResult(
    AGamePlatformArenaGameMode* GameMode,
    TFunction<void(bool, const FString&)> Completion)
{
    check(IsInGameThread());
    if (bResultSequenceClosed || ResultSubmission.IsSet()) { return; }
    UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
    if (!World || World->bIsTearingDown || World->GetAuthGameMode() != GameMode ||
        GameMode->GetMatchPhase() != EGamePlatformArenaMatchPhase::ResultPending)
    {
        Completion(false, TEXT("No authoritative world with a pending MatchResult."));
        return;
    }

    const FGamePlatformArenaMatchResult& Result = GameMode->GetPendingResult();
    if (Result.MatchId.IsEmpty() || Result.GameServerId.IsEmpty() || Result.EndRevision <= 0)
    {
        Completion(false, TEXT("Pending MatchResult identity is invalid."));
        return;
    }

    auto& Submission = ResultSubmission.Emplace();
    Submission.World = World;
    Submission.GameMode = GameMode;
    Submission.FrozenResult = Result;
    Submission.Completion = MoveTemp(Completion);
    SubmitPendingResultAttempt();
}

void FGamePlatformArenaServerCoordinator::CancelPendingResult()
{
    check(IsInGameThread());
    bResultSequenceClosed = true;
    ++ResultGeneration;
    if (ResultSubmission.IsSet())
    {
        if (UWorld* World = ResultSubmission->World.Get())
        {
            World->GetTimerManager().ClearTimer(ResultSubmission->RetryTimer);
        }
        ResultSubmission.Reset();
    }
}

AGamePlatformArenaGameMode* FGamePlatformArenaServerCoordinator::GetCurrentResultMode() const
{
    if (!ResultSubmission.IsSet()) { return nullptr; }
    UWorld* World = ResultSubmission->World.Get();
    AGamePlatformArenaGameMode* Mode = ResultSubmission->GameMode.Get();
    if (!World || World->bIsTearingDown || !Mode || Mode->GetWorld() != World ||
        World->GetAuthGameMode() != Mode || Mode->GetMatchPhase() != EGamePlatformArenaMatchPhase::ResultPending)
    {
        return nullptr;
    }

    const auto& Current = Mode->GetPendingResult();
    const auto& Frozen = ResultSubmission->FrozenResult;
    return Current.MatchId == Frozen.MatchId && Current.GameServerId == Frozen.GameServerId &&
        Current.EndRevision == Frozen.EndRevision ? Mode : nullptr;
}

void FGamePlatformArenaServerCoordinator::SubmitPendingResultAttempt()
{
    if (!GetCurrentResultMode()) { return; }
    const uint64 AttemptGeneration = ++ResultGeneration;
    BackendClient->SubmitMatchResult(
        ResultSubmission->FrozenResult,
        [Weak = AsWeak(), AttemptGeneration](EGamePlatformArenaSubmitStatus Status, const FString& Error)
        {
            if (const auto Self = Weak.Pin())
            {
                Self->HandleResultResponse(AttemptGeneration, Status, Error);
            }
        });
}

void FGamePlatformArenaServerCoordinator::HandleResultResponse(
    uint64 AttemptGeneration, EGamePlatformArenaSubmitStatus Status, const FString& Error)
{
    check(IsInGameThread());
    if (AttemptGeneration != ResultGeneration) { return; }
    AGamePlatformArenaGameMode* Mode = GetCurrentResultMode();
    if (!Mode) { return; }

    if (Status == EGamePlatformArenaSubmitStatus::RetryableFailure)
    {
        // 消费本次HTTP回调，仅让当前世界的一次性定时器发起下一次提交。
        const uint64 RetryGeneration = ++ResultGeneration;
        const float Delay = ResultSubmission->NextRetryDelaySeconds;
        ResultSubmission->NextRetryDelaySeconds = FMath::Min(Delay * 2.0f, MaxSettlementRetryDelaySeconds);
        Mode->GetWorld()->GetTimerManager().SetTimer(ResultSubmission->RetryTimer,
            FTimerDelegate::CreateLambda([Weak = AsWeak(), RetryGeneration]
            {
                if (const auto Self = Weak.Pin(); Self && Self->ResultGeneration == RetryGeneration)
                {
                    Self->SubmitPendingResultAttempt();
                }
            }), Delay, false);
        UE_LOG(LogTemp, Warning, TEXT("MainArena result acknowledgement pending; retrying in %.0fs. %s"), Delay, *Error);
        return;
    }

    auto Completion = MoveTemp(ResultSubmission->Completion);
    CancelPendingResult();
    if (Status != EGamePlatformArenaSubmitStatus::Committed &&
        Status != EGamePlatformArenaSubmitStatus::AlreadyCommitted)
    {
        Completion(false, Error.IsEmpty() ? TEXT("MatchResult submission failed.") : Error);
        return;
    }

    // 阶段变化可能同步触发世界退出。撤销后的旧回执不能启动安全排空。
    const uint64 CompletionGeneration = ResultGeneration;
    FString CommitError;
    const bool bCommitted = Mode->MarkResultCommitted(CommitError);
    if (CompletionGeneration == ResultGeneration)
    {
        Completion(bCommitted, CommitError);
    }
}
