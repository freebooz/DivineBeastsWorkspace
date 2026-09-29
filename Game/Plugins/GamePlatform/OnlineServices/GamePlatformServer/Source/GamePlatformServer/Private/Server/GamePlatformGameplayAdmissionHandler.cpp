#include "Server/GamePlatformGameplayAdmissionHandler.h"

#include "GameFramework/PlayerController.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"

void FGamePlatformServerGameplayAdmissionHandler::ValidateAdmissionProof(
    APlayerController& Controller,
    FGamePlatformAdmissionProofEnvelope Proof,
    FGamePlatformAdmissionProofCompletion Completion)
{
    check(IsInGameThread());

    UGameInstance* Instance = Controller.GetGameInstance();
    UGamePlatformServerAdmissionSubsystem* Admission =
        Instance
            ? Instance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>()
            : nullptr;

    if (!Admission || !Proof.IsStructurallyValid() || !Completion)
    {
        Proof.ResetSensitive();
        if (Completion)
        {
            FGamePlatformAdmissionProofResult Result;
            Result.ErrorCode = TEXT("ServerAdmissionUnavailable");
            Completion(MoveTemp(Result));
        }
        return;
    }

    const FGuid OperationId = Proof.OperationId;

    FGamePlatformServerAdmissionProof ServerProof;
    ServerProof.OperationId = Proof.OperationId;
    ServerProof.ReservationId = MoveTemp(Proof.ReservationId);
    ServerProof.AttemptId = MoveTemp(Proof.AttemptId);
    ServerProof.Credential = MoveTemp(Proof.Credential);
    Proof.ResetSensitive();

    // BeginAdmission在所有同步拒绝路径都会执行Completion；
    // 这里不能根据返回值再次完成，否则会形成双回调和重复客户端拒绝。
    Admission->BeginAdmission(
        Controller,
        MoveTemp(ServerProof),
        [OperationId, Completion = MoveTemp(Completion)](
            FGamePlatformServerAdmissionResult AdmissionResult) mutable
        {
            FGamePlatformAdmissionProofResult Result;
            Result.bSucceeded = AdmissionResult.bSucceeded;
            Result.ErrorCode = AdmissionResult.ErrorCode;

            if (AdmissionResult.bSucceeded)
            {
                const FGamePlatformServerVerifiedAdmission& Verified =
                    AdmissionResult.Admission;
                Result.Confirmation.OperationId = OperationId;
                Result.Confirmation.AssignmentId = Verified.AssignmentId;
                Result.Confirmation.GameSessionId = Verified.GameSessionId;
                Result.Confirmation.ServerInstanceId =
                    Verified.ServerInstanceId;
                Result.Confirmation.ServerBootId = Verified.ServerBootId;
                Result.Confirmation.WorldId = Verified.WorldId;
                Result.Confirmation.ProtocolVersion =
                    Verified.ProtocolVersion;
                Result.Confirmation.SessionEpoch =
                    static_cast<int64>(Verified.SessionEpoch);

                if (!Result.Confirmation.IsStructurallyValid())
                {
                    Result.bSucceeded = false;
                    Result.ErrorCode =
                        TEXT("ServerAdmissionConfirmationInvalid");
                    Result.Confirmation = {};
                }
            }

            Completion(MoveTemp(Result));
        });
}

void FGamePlatformServerGameplayAdmissionHandler::ReleaseController(
    APlayerController& Controller)
{
    check(IsInGameThread());

    UGameInstance* Instance = Controller.GetGameInstance();
    UGamePlatformServerAdmissionSubsystem* Admission =
        Instance
            ? Instance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>()
            : nullptr;
    if (!Admission)
    {
        return;
    }

    Admission->ReleaseAdmission(
        Controller,
        [](FGamePlatformServerAdmissionResult)
        {
            // 释放是连接清理路径；结果只用于内部诊断，不向已断开的客户端回包。
        });
}
