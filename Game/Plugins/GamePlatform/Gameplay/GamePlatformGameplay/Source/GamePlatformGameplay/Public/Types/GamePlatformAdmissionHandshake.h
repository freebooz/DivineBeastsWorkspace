#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "GamePlatformAdmissionHandshake.generated.h"

class APlayerController;

/**
 * FGamePlatformAdmissionProofEnvelope（平台准入证明信封）。
 *
 * Credential（凭据）仅用于一次可靠RPC传递，不允许记录、保存到PlayerState或复制给其他客户端。
 * ReservationId/AttemptId只用于幂等与诊断，不构成认证依据。
 */
USTRUCT()
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformAdmissionProofEnvelope
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid OperationId;

    UPROPERTY()
    FString ReservationId;

    UPROPERTY()
    FString AttemptId;

    UPROPERTY()
    TArray<uint8> Credential;

    bool IsStructurallyValid() const;
    void ResetSensitive();
};

/**
 * FGamePlatformAdmissionConfirmation（平台准入确认）。
 *
 * 只包含后端和目标Dedicated Server共同确认的非敏感Binding（连接绑定）；
 * 客户端必须与发起Transfer时收到的ExpectedBinding逐字段比较。
 */
USTRUCT()
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformAdmissionConfirmation
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid OperationId;

    UPROPERTY()
    FString AssignmentId;

    UPROPERTY()
    FString GameSessionId;

    UPROPERTY()
    FString ServerInstanceId;

    UPROPERTY()
    FString ServerBootId;

    UPROPERTY()
    FString WorldId;

    UPROPERTY()
    FString ProtocolVersion;

    UPROPERTY()
    int64 SessionEpoch = 0;

    bool IsStructurallyValid() const;
};

/** FGamePlatformAdmissionProofResult（准入证明处理结果）。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformAdmissionProofResult
{
    bool bSucceeded = false;
    FName ErrorCode = NAME_None;
    FGamePlatformAdmissionConfirmation Confirmation;
};

using FGamePlatformAdmissionProofCompletion =
    TFunction<void(FGamePlatformAdmissionProofResult)>;

/**
 * IGamePlatformGameplayAdmissionProofHandler（平台玩法准入证明处理器）。
 *
 * GamePlatformGameplay只负责双端RPC承载，不验证后端票据。
 * 服务器侧插件注册唯一实现完成真实验票；缺失或多实现时Fail Closed。
 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayAdmissionProofHandler
    : public IModularFeature
{
public:
    virtual ~IGamePlatformGameplayAdmissionProofHandler() = default;

    static FName GetModularFeatureName();

    virtual void ValidateAdmissionProof(
        APlayerController& Controller,
        FGamePlatformAdmissionProofEnvelope Proof,
        FGamePlatformAdmissionProofCompletion Completion) = 0;

    /** Controller断开时释放当前服务端准入投影；重复释放必须安全。 */
    virtual void ReleaseController(APlayerController& Controller) = 0;
};
