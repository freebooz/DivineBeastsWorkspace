#pragma once

#include "CoreMinimal.h"
#include "Backend/GamePlatformArenaServerBackendClient.h"
#include "Engine/TimerHandle.h"

class AGamePlatformArenaGameMode;
class IGamePlatformArenaServerProjectExtension;
class UWorld;

/** FGamePlatformArenaServerCoordinator（MainArena服务器编排适配）。
 *  负责把Assignment/Ticket/Result后端事实接到GameMode，不复制GameServer Registry职责。
 */
class FGamePlatformArenaServerCoordinator
    : public TSharedFromThis<FGamePlatformArenaServerCoordinator>
{
public:
    explicit FGamePlatformArenaServerCoordinator(
        TSharedRef<FGamePlatformArenaServerBackendClient> InBackendClient);

    void SetProjectExtension(IGamePlatformArenaServerProjectExtension* InExtension)
    {
        ProjectExtension = InExtension;
    }

    void LoadAndApplyAssignment(
        AGamePlatformArenaGameMode* GameMode,
        TFunction<void(bool bSuccess, const FString& Error)> Completion);

    /** 游戏线程提交当前世界唯一冻结结果。可恢复故障退避重放，直到可信回执、终止错误或世界撤销。 */
    void SubmitPendingResult(AGamePlatformArenaGameMode* GameMode,
        TFunction<void(bool bCommitted, const FString& Error)> Completion);

    /** 游戏线程关闭本世界结算并废弃旧回调，由WorldSubsystem随后取消其HTTP请求。 */
    void CancelPendingResult();

private:
    struct FResultSubmission
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AGamePlatformArenaGameMode> GameMode;
        FGamePlatformArenaMatchResult FrozenResult;
        TFunction<void(bool, const FString&)> Completion;
        FTimerHandle RetryTimer;
        float NextRetryDelaySeconds = 1.0f;
    };

    AGamePlatformArenaGameMode* GetCurrentResultMode() const;
    void SubmitPendingResultAttempt();
    void HandleResultResponse(uint64 AttemptGeneration, EGamePlatformArenaSubmitStatus Status, const FString& Error);

    IGamePlatformArenaServerProjectExtension* ProjectExtension = nullptr;
    TSharedRef<FGamePlatformArenaServerBackendClient> BackendClient;
    TOptional<FResultSubmission> ResultSubmission;
    uint64 ResultGeneration = 0;
    bool bResultSequenceClosed = false;
};
