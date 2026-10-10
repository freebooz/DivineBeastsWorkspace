#pragma once

#include "Server/GamePlatformServerAdmissionSubsystem.h"

#include "HAL/CriticalSection.h"
#include "Interfaces/IHttpRequest.h"

/**
 * FGamePlatformHttpAdmissionProvider（游戏平台HTTP准入提供者）。
 *
 * Dedicated Server通过受保护的GameServerControl内部接口验证一次性TransferTicket。
 * 原始Credential只存在于一次HTTP请求生命周期，不写日志、不保存到VerifiedAdmission。
 */
class FGamePlatformHttpAdmissionProvider final
    : public IGamePlatformServerAdmissionProvider
{
public:
    /** 析构前幂等关闭自有请求；游戏线程释放，完成委托不会越过Provider生命周期。 */
    virtual ~FGamePlatformHttpAdmissionProvider() override;
    /** 停止接纳，先解绑全部委托/取消HTTP，再以取消终态完成自有操作；不持锁调用外部代码。 */
    void Shutdown();
    virtual void ValidateAdmission(
        const FGamePlatformServerAdmissionTarget& Target,
        FGuid ConnectionId,
        uint64 ConnectionGeneration,
        FGamePlatformServerAdmissionProof Proof,
        FGamePlatformServerAdmissionCompletion Completion) override;

    virtual void ReleaseAdmission(
        const FGamePlatformServerVerifiedAdmission& Admission,
        FGamePlatformServerAdmissionCompletion Completion) override;

    virtual void CancelOperation(const FGuid& OperationId) override;

private:
    void TrackRequest(const FGuid& OperationId, const FHttpRequestPtr& Request,
        FGamePlatformServerAdmissionCompletion Completion);
    void UntrackRequest(const FGuid& OperationId);

    FCriticalSection RequestsMutex;
    /** 一个请求及共享一次性终态；不会强捕获Provider或Request本身。 */
    struct FTrackedRequest
    {
        FHttpRequestPtr Request;
        FGamePlatformServerAdmissionCompletion Completion;
    };
    TMap<FGuid, FTrackedRequest> Requests;
    bool bIsClosing = false;
    friend class FGamePlatformAdmissionShutdownTest;
    friend class FGamePlatformAdmissionCancelTest;
};
