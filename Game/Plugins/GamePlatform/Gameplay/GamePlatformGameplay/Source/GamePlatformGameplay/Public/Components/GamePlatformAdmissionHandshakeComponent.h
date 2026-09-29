#pragma once

#include "Components/ActorComponent.h"
#include "Types/GamePlatformAdmissionHandshake.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformAdmissionHandshakeComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAdmissionAcceptedNative,
    const FGamePlatformAdmissionConfirmation&);
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformAdmissionRejectedNative,
    FGuid,
    FName);

/**
 * UGamePlatformAdmissionHandshakeComponent（平台准入握手组件）。
 *
 * 服务器在真实PlayerController完成PostLogin后动态附加并复制到拥有客户端。
 * 客户端仅通过Reliable RPC提交一次性证明，禁止把TransferTicket放入Travel URL。
 */
UCLASS(NotBlueprintable, ClassGroup=(GamePlatform))
class GAMEPLATFORMGAMEPLAY_API UGamePlatformAdmissionHandshakeComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformAdmissionHandshakeComponent();

    /**
     * SubmitAdmissionProof（提交准入证明）。
     * 仅当前本地PlayerController可调用；成功仅表示RPC已发送，不等于服务端已准入。
     */
    FGamePlatformResult SubmitAdmissionProof(
        FGamePlatformAdmissionProofEnvelope Proof);

    FGamePlatformAdmissionAcceptedNative& OnAdmissionAccepted()
    {
        return AdmissionAccepted;
    }

    FGamePlatformAdmissionRejectedNative& OnAdmissionRejected()
    {
        return AdmissionRejected;
    }

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION(Server, Reliable)
    void ServerSubmitAdmissionProof(FGamePlatformAdmissionProofEnvelope Proof);

    UFUNCTION(Client, Reliable)
    void ClientAdmissionAccepted(FGamePlatformAdmissionConfirmation Confirmation);

    UFUNCTION(Client, Reliable)
    void ClientAdmissionRejected(FGuid OperationId, FName ErrorCode);

    static IGamePlatformGameplayAdmissionProofHandler* ResolveUniqueHandler();

    /** 服务端当前连接只允许一个准入操作飞行，防止同连接并发烧掉多张票据。 */
    bool bServerAdmissionInFlight = false;
    FGuid ServerActiveOperationId;
    FGamePlatformAdmissionConfirmation LastConfirmedAdmission;

    FGamePlatformAdmissionAcceptedNative AdmissionAccepted;
    FGamePlatformAdmissionRejectedNative AdmissionRejected;
};
