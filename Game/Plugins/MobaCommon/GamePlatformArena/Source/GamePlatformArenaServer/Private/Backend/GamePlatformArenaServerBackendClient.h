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
struct FGamePlatformArenaServerBackendConfig
{
    FString BaseUrl;
    FString InternalToken;
    FString GameServerId;
    /** 与唯一平台生命周期实际注册代次相同；不能另造Boot。 */
    FString ServerBootId;

    bool IsValid() const
    {
        return !BaseUrl.IsEmpty() && !InternalToken.IsEmpty() && !GameServerId.IsEmpty()&&!ServerBootId.IsEmpty();
    }
};

/** FGamePlatformArenaServerBackendClient（竞技服务器后端HTTP适配器）。
 *  只调用既有GameServerControlService内部接口，不保存/打印票据和内部Token。
 */
class FGamePlatformArenaServerBackendClient
    : public TSharedFromThis<FGamePlatformArenaServerBackendClient>
{
public:
    explicit FGamePlatformArenaServerBackendClient(FGamePlatformArenaServerBackendConfig InConfig);

    const FGamePlatformArenaServerBackendConfig& GetConfig() const { return Config; }

    void GetAssignment(
        TFunction<void(bool bSuccess, const FGamePlatformArenaAssignment& Assignment, const FString& Error)> Completion);

    /** 世界退出解绑并取消本适配器全部请求；不撤销已经提交的后端结果。 */
    void CancelAll();

    void SubmitMatchResult(
        const FGamePlatformArenaMatchResult& Result,
        TFunction<void(EGamePlatformArenaSubmitStatus Status, const FString& Error)> Completion);

private:
    FString MakeUrl(const FString& RelativePath) const;
    void ApplyCommonHeaders(const TSharedRef<class IHttpRequest, ESPMode::ThreadSafe>& Request) const;

    void TrackRequest(const TSharedRef<class IHttpRequest,ESPMode::ThreadSafe>& Request);
    FGamePlatformArenaServerBackendConfig Config;
    TArray<TSharedPtr<class IHttpRequest,ESPMode::ThreadSafe>> Requests;
};
