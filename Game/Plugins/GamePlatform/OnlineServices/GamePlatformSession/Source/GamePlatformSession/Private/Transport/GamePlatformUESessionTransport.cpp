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

bool IsSameTransportBinding(
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
    : public IGamePlatformSessionTransport,
      public TSharedFromThis<FGamePlatformUESessionTransport>
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

        // 绑定回调可能同步发起CancelTransfer、切换账号或更换Transport。
        // 每个不可回滚边界前核验原操作仍有效，禁止取消后依然ClientTravel。
        const FGuid BegunOperationId = Request.TransferOperationId;
        if (Callbacks.OnBindingPrepared)
        {
            // 回调可同步取消操作并清空Callbacks；把正在执行的TFunction临时移出，
            // 使取消时不会析构其调用栈中的可调用对象，且不恢复已失效的旧回调。
            auto Prepared = MoveTemp(Callbacks.OnBindingPrepared);
            Prepared(ExpectedBinding);
            if (ActiveOperationId == BegunOperationId && !Callbacks.OnBindingPrepared)
            {
                Callbacks.OnBindingPrepared = MoveTemp(Prepared);
            }
        }
        if (ActiveOperationId != BegunOperationId)
        {
            return;
        }

        // Commit回调必须发生在ClientTravel之前，确保Session先跨过旅行边界。
        if (Callbacks.OnTravelCommitted)
        {
            auto Committed = MoveTemp(Callbacks.OnTravelCommitted);
            Committed(ExpectedBinding);
            if (ActiveOperationId == BegunOperationId && !Callbacks.OnTravelCommitted)
            {
                Callbacks.OnTravelCommitted = MoveTemp(Committed);
            }
        }
        if (ActiveOperationId != BegunOperationId)
        {
            return;
        }
        bTravelCommitted = true;

        // Endpoint已经由Session Validate严格禁止URL Query/Fragment；
        // TransferTicket绝不拼进URL，也不进入日志。
        Controller->ClientTravel(
            Request.Endpoint,
            TRAVEL_Absolute,
            false);
        // ClientTravel有可能同步广播旅行/网络故障并清空原操作，不能再创建幽灵握手Ticker。
        if (ActiveOperationId != BegunOperationId)
        {
            return;
        }

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
            !IsSameTransportBinding(ExpectedBinding, Binding))
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
            IsSameTransportBinding(CurrentBinding, Binding))
        {
            CurrentBinding = {};
            UnbindNetworkFailure();
            DisconnectCurrentWorld();
        }
    }

private:
    bool TickHandshake(float)
    {
        // 原生Ticker调用中OnFact可能同步替换Transport；自持引用防止原对象析构。
        const TSharedRef<FGamePlatformUESessionTransport> CallbackLifetime = AsShared();
        (void)CallbackLifetime;
        const FGuid ProbeOperationId = ActiveOperationId;
        if (!ProbeOperationId.IsValid())
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
                // OnFact可能同步完成/撤销传输并清空Callbacks，不能在被移除的
                // TFunction实例内继续运行。局部可调用对象延迟到回调返回后才销毁。
                auto FactCallback = MoveTemp(Callbacks.OnFact);
                FactCallback(EGamePlatformSessionTransferFact::NetworkConnected, ExpectedBinding);
                if (ActiveOperationId == ProbeOperationId && !Callbacks.OnFact)
                {
                    Callbacks.OnFact = MoveTemp(FactCallback);
                }
            }
        }
        // 网络事实回调可能同步取消本次连接，禁止仍提交一次性准入凭据。
        if (ActiveOperationId != ProbeOperationId)
        {
            return false;
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

        // RPC有可能同步触发拒绝回调和清理，不恢复已取消操作的提交状态。
        if (ActiveOperationId != ProbeOperationId)
        {
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
            // AdmissionConfirmed可能是第4个可信事实，Session随即CompleteTransfer
            // 清空回调容器。必须保证当前OnFact函数对象在同步完成期间不被销毁。
            const TSharedRef<FGamePlatformUESessionTransport> CallbackLifetime = AsShared();
            (void)CallbackLifetime;
            const FGuid ConfirmedOperationId = ActiveOperationId;
            auto FactCallback = MoveTemp(Callbacks.OnFact);
            FactCallback(EGamePlatformSessionTransferFact::AdmissionConfirmed, ExpectedBinding);
            if (ActiveOperationId == ConfirmedOperationId && !Callbacks.OnFact)
            {
                Callbacks.OnFact = MoveTemp(FactCallback);
            }
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
