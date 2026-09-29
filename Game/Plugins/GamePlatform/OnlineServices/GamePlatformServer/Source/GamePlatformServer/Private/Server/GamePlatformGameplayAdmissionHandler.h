#pragma once

#include "Types/GamePlatformAdmissionHandshake.h"

/**
 * FGamePlatformServerGameplayAdmissionHandler（服务器玩法准入桥接器）。
 *
 * 负责把共享RPC层收到的一次性证明交给GamePlatformServer Admission Subsystem，
 * 并把成功结果转换成客户端可核对的非敏感Confirmation。
 */
class FGamePlatformServerGameplayAdmissionHandler final
    : public IGamePlatformGameplayAdmissionProofHandler
{
public:
    virtual void ValidateAdmissionProof(
        APlayerController& Controller,
        FGamePlatformAdmissionProofEnvelope Proof,
        FGamePlatformAdmissionProofCompletion Completion) override;

    virtual void ReleaseController(APlayerController& Controller) override;
};
