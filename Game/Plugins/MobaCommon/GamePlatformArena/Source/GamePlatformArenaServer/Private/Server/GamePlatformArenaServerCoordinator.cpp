#include "Server/GamePlatformArenaServerCoordinator.h"

#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"

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
    TFunction<void(bool, const FString&)> Completion,
    int32 MaxAttempts)
{
    if (GameMode == nullptr) { Completion(false, TEXT("GameMode missing.")); return; }
    const FGamePlatformArenaMatchResult Result = GameMode->GetPendingResult();
    if (Result.MatchId.IsEmpty()) { Completion(false, TEXT("No pending MatchResult.")); return; }
    SubmitPendingResultAttempt(GameMode, Result, 1, FMath::Clamp(MaxAttempts, 1, 5), MoveTemp(Completion));
}

void FGamePlatformArenaServerCoordinator::SubmitPendingResultAttempt(
    TWeakObjectPtr<AGamePlatformArenaGameMode> GameMode,
    FGamePlatformArenaMatchResult Result,
    int32 Attempt,
    int32 MaxAttempts,
    TFunction<void(bool, const FString&)> Completion)
{
    BackendClient->SubmitMatchResult(
        Result,
        [Self = AsShared(), GameMode, Result, Attempt, MaxAttempts, Completion = MoveTemp(Completion)](EGamePlatformArenaSubmitStatus Status, const FString& Error) mutable
        {
            AGamePlatformArenaGameMode* Mode = GameMode.Get();
            if (Mode == nullptr) { Completion(false, TEXT("GameMode destroyed while result pending.")); return; }
            if (Status == EGamePlatformArenaSubmitStatus::Committed || Status == EGamePlatformArenaSubmitStatus::AlreadyCommitted)
            {
                FString CommitError;
                Completion(Mode->MarkResultCommitted(CommitError), CommitError);
                return;
            }
            if (Status == EGamePlatformArenaSubmitStatus::RetryableFailure && Attempt < MaxAttempts)
            {
                // 使用同一个matchId和完全相同的Result重试，依赖后端幂等；不会生成第二份结算。
                Self->SubmitPendingResultAttempt(GameMode, Result, Attempt + 1, MaxAttempts, MoveTemp(Completion));
                return;
            }
            Completion(false, Error.IsEmpty() ? TEXT("MatchResult submission failed.") : Error);
        });
}
