#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaTypes.h"

/** EGamePlatformArenaSubmitStatus（比赛结果提交状态）。 */
enum class EGamePlatformArenaSubmitStatus : uint8
{
    Committed,
    AlreadyCommitted,
    Conflict,
    RetryableFailure,
    TerminalFailure
};

/** FGamePlatformArenaServerBackendConfig（MainArena服务器后端连接配置）。 */
struct GAMEPLATFORMARENASERVER_API FGamePlatformArenaServerBackendConfig
{
    FString BaseUrl;
    FString InternalToken;
    FString GameServerId;

    bool IsValid() const
    {
        return !BaseUrl.IsEmpty() && !InternalToken.IsEmpty() && !GameServerId.IsEmpty();
    }
};

/** FGamePlatformArenaServerBackendClient（竞技服务器后端HTTP适配器）。
 *  只调用既有GameServerControlService内部接口，不保存/打印票据和内部Token。
 */
class GAMEPLATFORMARENASERVER_API FGamePlatformArenaServerBackendClient
    : public TSharedFromThis<FGamePlatformArenaServerBackendClient>
{
public:
    explicit FGamePlatformArenaServerBackendClient(FGamePlatformArenaServerBackendConfig InConfig);

    const FGamePlatformArenaServerBackendConfig& GetConfig() const { return Config; }

    void GetAssignment(
        TFunction<void(bool bSuccess, const FGamePlatformArenaAssignment& Assignment, const FString& Error)> Completion);

    void ValidateAndConsumeTransferTicket(
        const FString& TransferTicket,
        const FString& PlayerId,
        const FString& MatchId,
        TFunction<void(bool bSuccess, const FGamePlatformArenaTransferTicketClaims& Claims, const FString& Error)> Completion);

    void SubmitMatchResult(
        const FGamePlatformArenaMatchResult& Result,
        TFunction<void(EGamePlatformArenaSubmitStatus Status, const FString& Error)> Completion);

private:
    FString MakeUrl(const FString& RelativePath) const;
    void ApplyCommonHeaders(const TSharedRef<class IHttpRequest, ESPMode::ThreadSafe>& Request) const;

    FGamePlatformArenaServerBackendConfig Config;
};
