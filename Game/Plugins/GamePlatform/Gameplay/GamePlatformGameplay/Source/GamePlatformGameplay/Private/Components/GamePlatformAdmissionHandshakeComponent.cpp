#include "Components/GamePlatformAdmissionHandshakeComponent.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/PlayerController.h"
#include "HAL/UnrealMemory.h"

namespace
{
constexpr int32 MaxAdmissionTextChars = 1024;
constexpr int32 MinAdmissionCredentialBytes = 32;
constexpr int32 MaxAdmissionCredentialBytes = 16 * 1024;

bool IsBoundedAdmissionText(const FString& Value)
{
    return !Value.IsEmpty() && Value.Len() <= MaxAdmissionTextChars &&
        !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n"));
}
}

bool FGamePlatformAdmissionProofEnvelope::IsStructurallyValid() const
{
    return OperationId.IsValid() &&
        IsBoundedAdmissionText(ReservationId) &&
        IsBoundedAdmissionText(AttemptId) &&
        Credential.Num() >= MinAdmissionCredentialBytes &&
        Credential.Num() <= MaxAdmissionCredentialBytes;
}

void FGamePlatformAdmissionProofEnvelope::ResetSensitive()
{
    if (!Credential.IsEmpty())
    {
        FMemory::Memzero(Credential.GetData(), Credential.Num());
        Credential.Reset();
    }
}

bool FGamePlatformAdmissionConfirmation::IsStructurallyValid() const
{
    return OperationId.IsValid() &&
        IsBoundedAdmissionText(AssignmentId) &&
        IsBoundedAdmissionText(GameSessionId) &&
        IsBoundedAdmissionText(ServerInstanceId) &&
        IsBoundedAdmissionText(ServerBootId) &&
        IsBoundedAdmissionText(WorldId) &&
        IsBoundedAdmissionText(ProtocolVersion) &&
        SessionEpoch > 0;
}

FName IGamePlatformGameplayAdmissionProofHandler::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformGameplayAdmissionProofHandler"));
}

UGamePlatformAdmissionHandshakeComponent::UGamePlatformAdmissionHandshakeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

FGamePlatformResult
UGamePlatformAdmissionHandshakeComponent::SubmitAdmissionProof(
    FGamePlatformAdmissionProofEnvelope Proof)
{
    check(IsInGameThread());

    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller || !Controller->IsLocalController() ||
        !Proof.IsStructurallyValid())
    {
        Proof.ResetSensitive();
        return FGamePlatformResult::Failure(
            TEXT("AdmissionProofInvalid"),
            TEXT("准入证明结构无效或组件不属于当前本地PlayerController。"));
    }

    ServerSubmitAdmissionProof(MoveTemp(Proof));
    return FGamePlatformResult::Success();
}

void UGamePlatformAdmissionHandshakeComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (APlayerController* Controller = Cast<APlayerController>(GetOwner());
        Controller && Controller->HasAuthority())
    {
        if (IGamePlatformGameplayAdmissionProofHandler* Handler =
                ResolveUniqueHandler())
        {
            Handler->ReleaseController(*Controller);
        }
    }

    bServerAdmissionInFlight = false;
    ServerActiveOperationId.Invalidate();
    LastConfirmedAdmission = {};
    AdmissionAccepted.Clear();
    AdmissionRejected.Clear();
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformAdmissionHandshakeComponent::ServerSubmitAdmissionProof_Implementation(
    FGamePlatformAdmissionProofEnvelope Proof)
{
    check(IsInGameThread());

    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller || !Controller->HasAuthority() ||
        !Proof.IsStructurallyValid())
    {
        const FGuid OperationId = Proof.OperationId;
        Proof.ResetSensitive();
        ClientAdmissionRejected(
            OperationId,
            TEXT("AdmissionProofInvalid"));
        return;
    }

    if (LastConfirmedAdmission.IsStructurallyValid())
    {
        if (LastConfirmedAdmission.OperationId == Proof.OperationId)
        {
            Proof.ResetSensitive();
            ClientAdmissionAccepted(LastConfirmedAdmission);
        }
        else
        {
            Proof.ResetSensitive();
            ClientAdmissionRejected(
                Proof.OperationId,
                TEXT("AdmissionAlreadyConfirmed"));
        }
        return;
    }

    if (bServerAdmissionInFlight)
    {
        const FGuid OperationId = Proof.OperationId;
        Proof.ResetSensitive();
        ClientAdmissionRejected(
            OperationId,
            TEXT("AdmissionBusy"));
        return;
    }

    IGamePlatformGameplayAdmissionProofHandler* Handler =
        ResolveUniqueHandler();
    if (!Handler)
    {
        const FGuid OperationId = Proof.OperationId;
        Proof.ResetSensitive();
        ClientAdmissionRejected(
            OperationId,
            TEXT("AdmissionHandlerUnavailable"));
        return;
    }

    bServerAdmissionInFlight = true;
    ServerActiveOperationId = Proof.OperationId;
    const FGuid ExpectedOperationId = Proof.OperationId;
    const TWeakObjectPtr<UGamePlatformAdmissionHandshakeComponent> WeakThis(this);

    Handler->ValidateAdmissionProof(
        *Controller,
        MoveTemp(Proof),
        [WeakThis, ExpectedOperationId](
            FGamePlatformAdmissionProofResult Result) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis, ExpectedOperationId, Result = MoveTemp(Result)]() mutable
                {
                    UGamePlatformAdmissionHandshakeComponent* Self =
                        WeakThis.Get();
                    if (!Self ||
                        !Self->bServerAdmissionInFlight ||
                        Self->ServerActiveOperationId != ExpectedOperationId)
                    {
                        return;
                    }

                    Self->bServerAdmissionInFlight = false;
                    Self->ServerActiveOperationId.Invalidate();

                    if (!Result.bSucceeded ||
                        !Result.Confirmation.IsStructurallyValid() ||
                        Result.Confirmation.OperationId != ExpectedOperationId)
                    {
                        Self->ClientAdmissionRejected(
                            ExpectedOperationId,
                            Result.ErrorCode.IsNone()
                                ? FName(TEXT("AdmissionRejected"))
                                : Result.ErrorCode);
                        return;
                    }

                    Self->LastConfirmedAdmission =
                        MoveTemp(Result.Confirmation);
                    Self->ClientAdmissionAccepted(
                        Self->LastConfirmedAdmission);
                });
        });
}

void UGamePlatformAdmissionHandshakeComponent::ClientAdmissionAccepted_Implementation(
    FGamePlatformAdmissionConfirmation Confirmation)
{
    if (!Confirmation.IsStructurallyValid())
    {
        AdmissionRejected.Broadcast(
            Confirmation.OperationId,
            TEXT("AdmissionConfirmationInvalid"));
        return;
    }
    AdmissionAccepted.Broadcast(Confirmation);
}

void UGamePlatformAdmissionHandshakeComponent::ClientAdmissionRejected_Implementation(
    FGuid OperationId,
    FName ErrorCode)
{
    AdmissionRejected.Broadcast(
        OperationId,
        ErrorCode.IsNone()
            ? FName(TEXT("AdmissionRejected"))
            : ErrorCode);
}

IGamePlatformGameplayAdmissionProofHandler*
UGamePlatformAdmissionHandshakeComponent::ResolveUniqueHandler()
{
    IModularFeatures& Features = IModularFeatures::Get();
    const TArray<IGamePlatformGameplayAdmissionProofHandler*> Handlers =
        Features.GetModularFeatureImplementations<
            IGamePlatformGameplayAdmissionProofHandler>(
                IGamePlatformGameplayAdmissionProofHandler::
                    GetModularFeatureName());
    return Handlers.Num() == 1 ? Handlers[0] : nullptr;
}
