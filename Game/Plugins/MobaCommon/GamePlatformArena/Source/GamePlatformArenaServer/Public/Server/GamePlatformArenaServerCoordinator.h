#pragma once

#include "CoreMinimal.h"
#include "Backend/GamePlatformArenaServerBackendClient.h"

class AGamePlatformArenaGameMode;
class AGamePlatformArenaPlayerState;
class IGamePlatformArenaServerProjectExtension;

/** FGamePlatformArenaServerCoordinator（MainArena服务器编排适配）。
 *  负责把Assignment/Ticket/Result后端事实接到GameMode，不复制GameServer Registry职责。
 */
class GAMEPLATFORMARENASERVER_API FGamePlatformArenaServerCoordinator
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

    void ValidateTicketAndAdmit(
        AGamePlatformArenaGameMode* GameMode,
        AGamePlatformArenaPlayerState* PlayerState,
        const FString& PlayerId,
        const FString& TransferTicket,
        TFunction<void(bool bSuccess, const FString& Error)> Completion);

    void SubmitPendingResult(
        AGamePlatformArenaGameMode* GameMode,
        TFunction<void(bool bCommitted, const FString& Error)> Completion,
        int32 MaxAttempts = 3);

private:
    void SubmitPendingResultAttempt(
        TWeakObjectPtr<AGamePlatformArenaGameMode> GameMode,
        FGamePlatformArenaMatchResult Result,
        int32 Attempt,
        int32 MaxAttempts,
        TFunction<void(bool, const FString&)> Completion);

    IGamePlatformArenaServerProjectExtension* ProjectExtension = nullptr;
    TSharedRef<FGamePlatformArenaServerBackendClient> BackendClient;
};
