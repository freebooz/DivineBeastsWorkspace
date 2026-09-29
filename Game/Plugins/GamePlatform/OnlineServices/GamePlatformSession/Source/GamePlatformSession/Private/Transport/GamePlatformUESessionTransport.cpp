#include "Transport/GamePlatformUESessionTransport.h"

#include "Components/GamePlatformAdmissionHandshakeComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
constexpr float HandshakeProbeIntervalSeconds = 0.05f;

bool IsSameBinding(
    const FGamePlatformSessionConnectionBinding& Left,
    const FGamePlatformSessionConnectionBinding& Right)
{
    return Left.AssignmentId == Right.AssignmentId &&
        Left.GameSessionId == Right.GameSessionId &&
        Left.ServerInstanceId == Right.ServerInstanceId &&
        Left.ServerBootId == Right.ServerBootId &&
        Left.WorldId == Right.WorldId &&
        Left.ProtocolVersion == Right.ProtocolVersion &&
        Left.SessionEpoch == Right.SessionEpoch;
}

bool ConfirmationMatches(
    const FGamePlatformAdmissionConfirmation& Confirmation,
    const FGuid& OperationId,
    const FGamePlatformSessionConnectionBinding& Binding)
{
    return Confirmation.IsStructurallyValid() &&
        Confirmation.OperationId == OperationId &&
        Confirmation.AssignmentId == Binding.AssignmentId &&
        Confirmation.GameSessionId == Binding.GameSessionId &&
        Confirmation.ServerInstanceId == Binding.ServerInstanceId &&
        Confirmation.ServerBootId == Binding.ServerBootId &&
        Confirmation.WorldId == Binding.WorldId.ToString() &&
        Confirmation.ProtocolVersion == Binding.ProtocolVersion &&
        Confirmation.SessionEpoch == Binding.SessionEpoch;
}

class FGamePlatformUESessionTransport final
    : public IGamePlatformSessionTransport
{
public:
    explicit FGamePlatformUESessionTransport(UGameInstance& InGameInstance)
        : GameInstance(&InGameInstance)
    {
    }

    virtual ~FGamePlatformUESessionTransport() override
    {
        ClearOperation(false);
        UnbindNetworkFailure();
    }

    virtual void BeginTransfer(
        const FGamePlatformSessionTransferRequest& Request,
        FGamePlatformSessionTransportCallbacks InCallbacks) override
    {
        check(IsInGameThread());

        if (ActiveOperationId.IsValid())
        {
            if (InCallbacks.OnFailed)
            {
                InCallbacks.OnFailed(
                    TEXT("SessionTransportBusy"),
                    TEXT("UE会话Transport已有活动跨服操作。"));
            }
            return;
        }

        UGameInstance* Instance = GameInstance.Get();
        UWorld* World = Instance ? Instance->GetWorld() : nullptr;
        APlayerController* Controller =
            World ? World->GetFirstPlayerController() : nullptr;
        if (!Instance || !World || !Controller ||
            !Controller->IsLocalController() ||
            !Request.ExpectedBinding.IsValid() ||
            Request.TicketId.IsEmpty())
        {
            if (InCallbacks.OnFailed)
            {
                InCallbacks.OnFailed(
                    TEXT("SessionTransportContextInvalid"),
                    TEXT("当前客户端没有可用于安全旅行的本地PlayerController或权威Binding。"));
            }
            return;
        }

        FTCHARToUTF8 TicketUtf8(*Request.TransferTicket);
        if (TicketUtf8.Length() < 32 || TicketUtf8.Length() > 16 * 1024)
        {
            if (InCallbacks.OnFailed)
            {
                InCallbacks.OnFailed(
                    TEXT("SessionTransferCredentialInvalid"),
                    TEXT("一次性迁移票据长度不满足安全握手要求。"));
            }
            return;
        }

        ActiveOperationId = Request.TransferOperationId;
        ExpectedBinding = Request.ExpectedBinding;
        TicketId = Request.TicketId;
        Credential.Reset(TicketUtf8.Length());
        Credential.Append(
            reinterpret_cast<const uint8*>(TicketUtf8.Get()),
            TicketUtf8.Length());
        Callbacks = MoveTemp(InCallbacks);
        bTravelCommitted = false;
        bProofSubmitted = false;
        bNetworkFactSent = false;

        BindEngineFailures();

        if (Callbacks.OnBindingPrepared)
        {
            Callbacks.OnBindingPrepared(ExpectedBinding);
        }

        // Commit回调必须发生在ClientTravel之前，确保Session状态先跨过不可回滚边界。
        if (Callbacks.OnTravelCommitted)
        {
            Callbacks.OnTravelCommitted(ExpectedBinding);
        }
        bTravelCommitted = true;

        // Endpoint已经由Session Validate严格禁止URL Query/Fragment；
        // TransferTicket绝不拼进URL，也不进入日志。
        Controller->ClientTravel(
            Request.Endpoint,
            TRAVEL_Absolute,
            false);

        HandshakeTicker = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(
                this,
                &FGamePlatformUESessionTransport::TickHandshake),
            HandshakeProbeIntervalSeconds);
        if (!HandshakeTicker.IsValid())
        {
            FailActive(
                TEXT("SessionHandshakeScheduleFailed"),
                TEXT("无法调度服务器准入握手。"),
                true);
        }
    }

    virtual void CancelTransfer(
        const FGuid& TransferOperationId) override
    {
        check(IsInGameThread());

        if (!ActiveOperationId.IsValid() ||
            ActiveOperationId != TransferOperationId)
        {
            return;
        }

        const bool bDisconnect = bTravelCommitted;
        ClearOperation(false);
        if (bDisconnect)
        {
            DisconnectCurrentWorld();
        }
    }

    virtual void CompleteTransfer(
        const FGuid& TransferOperationId,
        const FGamePlatformSessionConnectionBinding& Binding) override
    {
        check(IsInGameThread());

        if (!ActiveOperationId.IsValid() ||
            ActiveOperationId != TransferOperationId ||
            !IsSameBinding(ExpectedBinding, Binding))
        {
            return;
        }

        CurrentBinding = Binding;
        ClearOperation(true);
        // Ready后继续保留NetworkFailure监听；真实断线会通过DisconnectedCallback回报Session。
        BindNetworkFailure();
    }

    virtual void SetDisconnectedCallback(
        FGamePlatformSessionDisconnectedCallback Callback) override
    {
        check(IsInGameThread());
        DisconnectedCallback = MoveTemp(Callback);
    }

    virtual void LeaveSession(
        const FGamePlatformSessionConnectionBinding& Binding) override
    {
        check(IsInGameThread());

        if (ActiveOperationId.IsValid())
        {
            ClearOperation(false);
        }

        if (CurrentBinding.IsValid() &&
            IsSameBinding(CurrentBinding, Binding))
        {
            CurrentBinding = {};
            UnbindNetworkFailure();
            DisconnectCurrentWorld();
        }
    }

private:
    bool TickHandshake(float)
    {
        if (!ActiveOperationId.IsValid())
        {
            HandshakeTicker.Reset();
            return false;
        }

        UGameInstance* Instance = GameInstance.Get();
        UWorld* World = Instance ? Instance->GetWorld() : nullptr;
        APlayerController* Controller =
            World ? World->GetFirstPlayerController() : nullptr;
        if (!Controller || !Controller->IsLocalController())
        {
            return true;
        }

        UGamePlatformAdmissionHandshakeComponent* Component =
            Controller->FindComponentByClass<
                UGamePlatformAdmissionHandshakeComponent>();
        if (!Component)
        {
            return true;
        }

        if (!bNetworkFactSent)
        {
            bNetworkFactSent = true;
            if (Callbacks.OnFact)
            {
                Callbacks.OnFact(
                    EGamePlatformSessionTransferFact::NetworkConnected,
                    ExpectedBinding);
            }
        }

        if (bProofSubmitted)
        {
            return true;
        }

        HandshakeComponent = Component;
        AdmissionAcceptedHandle =
            Component->OnAdmissionAccepted().AddRaw(
                this,
                &FGamePlatformUESessionTransport::HandleAdmissionAccepted);
        AdmissionRejectedHandle =
            Component->OnAdmissionRejected().AddRaw(
                this,
                &FGamePlatformUESessionTransport::HandleAdmissionRejected);

        FGamePlatformAdmissionProofEnvelope Proof;
        Proof.OperationId = ActiveOperationId;
        Proof.ReservationId = TicketId;
        // Session内部AttemptId不跨插件公开；OperationId在同一活动操作中稳定，
        // 这里只作为RPC幂等相关标识，真正授权完全依赖Credential的后端验签。
        Proof.AttemptId =
            ActiveOperationId.ToString(
                EGuidFormats::DigitsWithHyphensLower);
        Proof.Credential = MoveTemp(Credential);

        const FGamePlatformResult Submitted =
            Component->SubmitAdmissionProof(MoveTemp(Proof));
        if (!Submitted.IsSuccess())
        {
            FailActive(
                Submitted.Code.IsNone()
                    ? FName(TEXT("SessionAdmissionSubmitFailed"))
                    : Submitted.Code,
                TEXT("无法向目标服务器提交一次性准入证明。"),
                true);
            return false;
        }

        bProofSubmitted = true;
        return true;
    }

    void HandleAdmissionAccepted(
        const FGamePlatformAdmissionConfirmation& Confirmation)
    {
        check(IsInGameThread());

        if (!ActiveOperationId.IsValid())
        {
            return;
        }

        if (!ConfirmationMatches(
                Confirmation,
                ActiveOperationId,
                ExpectedBinding))
        {
            FailActive(
                TEXT("SessionAdmissionBindingMismatch"),
                TEXT("服务器准入确认与后端预期Binding不一致。"),
                true);
            return;
        }

        if (Callbacks.OnFact)
        {
            Callbacks.OnFact(
                EGamePlatformSessionTransferFact::AdmissionConfirmed,
                ExpectedBinding);
        }
    }

    void HandleAdmissionRejected(
        FGuid OperationId,
        FName ErrorCode)
    {
        check(IsInGameThread());

        if (!ActiveOperationId.IsValid() ||
            ActiveOperationId != OperationId)
        {
            return;
        }

        FailActive(
            ErrorCode.IsNone()
                ? FName(TEXT("SessionAdmissionRejected"))
                : ErrorCode,
            TEXT("目标服务器拒绝当前一次性准入证明。"),
            true);
    }

    void HandleTravelFailure(
        UWorld* World,
        ETravelFailure::Type,
        const FString&)
    {
        if (!IsRelevantWorld(World) ||
            !ActiveOperationId.IsValid())
        {
            return;
        }

        FailActive(
            TEXT("SessionTravelFailure"),
            TEXT("UE客户端旅行失败。"),
            false);
    }

    void HandleNetworkFailure(
        UWorld* World,
        UNetDriver*,
        ENetworkFailure::Type,
        const FString&)
    {
        if (!IsRelevantWorld(World) || bIntentionalDisconnect)
        {
            return;
        }

        if (ActiveOperationId.IsValid())
        {
            FailActive(
                TEXT("SessionNetworkFailure"),
                TEXT("会话连接期间发生网络故障。"),
                false);
            return;
        }

        if (CurrentBinding.IsValid())
        {
            const FGamePlatformSessionConnectionBinding Lost =
                CurrentBinding;
            CurrentBinding = {};
            if (DisconnectedCallback)
            {
                DisconnectedCallback(Lost);
            }
        }
    }

    bool IsRelevantWorld(const UWorld* World) const
    {
        const UGameInstance* Instance = GameInstance.Get();
        return Instance && (!World || World->GetGameInstance() == Instance);
    }

    void BindEngineFailures()
    {
        if (!GEngine)
        {
            return;
        }

        if (!TravelFailureHandle.IsValid())
        {
            TravelFailureHandle =
                GEngine->OnTravelFailure().AddRaw(
                    this,
                    &FGamePlatformUESessionTransport::
                        HandleTravelFailure);
        }
        BindNetworkFailure();
    }

    void BindNetworkFailure()
    {
        if (GEngine && !NetworkFailureHandle.IsValid())
        {
            NetworkFailureHandle =
                GEngine->OnNetworkFailure().AddRaw(
                    this,
                    &FGamePlatformUESessionTransport::
                        HandleNetworkFailure);
        }
    }

    void UnbindTravelFailure()
    {
        if (GEngine && TravelFailureHandle.IsValid())
        {
            GEngine->OnTravelFailure().Remove(TravelFailureHandle);
            TravelFailureHandle.Reset();
        }
    }

    void UnbindNetworkFailure()
    {
        if (GEngine && NetworkFailureHandle.IsValid())
        {
            GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
            NetworkFailureHandle.Reset();
        }
    }

    void UnbindHandshakeComponent()
    {
        if (UGamePlatformAdmissionHandshakeComponent* Component =
                HandshakeComponent.Get())
        {
            if (AdmissionAcceptedHandle.IsValid())
            {
                Component->OnAdmissionAccepted().Remove(
                    AdmissionAcceptedHandle);
            }
            if (AdmissionRejectedHandle.IsValid())
            {
                Component->OnAdmissionRejected().Remove(
                    AdmissionRejectedHandle);
            }
        }

        HandshakeComponent.Reset();
        AdmissionAcceptedHandle.Reset();
        AdmissionRejectedHandle.Reset();
    }

    void ClearOperation(bool bKeepNetworkFailure)
    {
        if (HandshakeTicker.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(
                HandshakeTicker);
            HandshakeTicker.Reset();
        }

        UnbindHandshakeComponent();
        UnbindTravelFailure();
        if (!bKeepNetworkFailure && !CurrentBinding.IsValid())
        {
            UnbindNetworkFailure();
        }

        if (!Credential.IsEmpty())
        {
            FMemory::Memzero(
                Credential.GetData(),
                Credential.Num());
            Credential.Reset();
        }

        ActiveOperationId.Invalidate();
        ExpectedBinding = {};
        TicketId.Reset();
        Callbacks = {};
        bTravelCommitted = false;
        bProofSubmitted = false;
        bNetworkFactSent = false;
    }

    void FailActive(
        FName ErrorCode,
        FString ErrorMessage,
        bool bDisconnect)
    {
        FGamePlatformSessionTransportCallbacks FailureCallbacks =
            MoveTemp(Callbacks);
        CurrentBinding = {};
        ClearOperation(false);

        if (bDisconnect)
        {
            DisconnectCurrentWorld();
        }

        if (FailureCallbacks.OnFailed)
        {
            FailureCallbacks.OnFailed(
                ErrorCode,
                MoveTemp(ErrorMessage));
        }
    }

    void DisconnectCurrentWorld()
    {
        UGameInstance* Instance = GameInstance.Get();
        if (!Instance || !Instance->GetWorld())
        {
            return;
        }

        bIntentionalDisconnect = true;
        Instance->ReturnToMainMenu();
        bIntentionalDisconnect = false;
    }

    TWeakObjectPtr<UGameInstance> GameInstance;
    FGuid ActiveOperationId;
    FGamePlatformSessionConnectionBinding ExpectedBinding;
    FGamePlatformSessionConnectionBinding CurrentBinding;
    FString TicketId;
    TArray<uint8> Credential;
    FGamePlatformSessionTransportCallbacks Callbacks;
    FGamePlatformSessionDisconnectedCallback DisconnectedCallback;

    TWeakObjectPtr<UGamePlatformAdmissionHandshakeComponent>
        HandshakeComponent;
    FDelegateHandle AdmissionAcceptedHandle;
    FDelegateHandle AdmissionRejectedHandle;
    FDelegateHandle TravelFailureHandle;
    FDelegateHandle NetworkFailureHandle;
    FTSTicker::FDelegateHandle HandshakeTicker;

    bool bTravelCommitted = false;
    bool bProofSubmitted = false;
    bool bNetworkFactSent = false;
    bool bIntentionalDisconnect = false;
};
}

TSharedPtr<IGamePlatformSessionTransport>
CreateGamePlatformUESessionTransport(UGameInstance& GameInstance)
{
    return MakeShared<FGamePlatformUESessionTransport>(GameInstance);
}
