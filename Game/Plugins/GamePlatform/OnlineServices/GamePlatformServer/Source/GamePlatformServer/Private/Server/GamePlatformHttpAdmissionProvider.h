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
    void TrackRequest(const FGuid& OperationId, const FHttpRequestPtr& Request);
    void UntrackRequest(const FGuid& OperationId);

    FCriticalSection RequestsMutex;
    TMap<FGuid, FHttpRequestPtr> Requests;
};
