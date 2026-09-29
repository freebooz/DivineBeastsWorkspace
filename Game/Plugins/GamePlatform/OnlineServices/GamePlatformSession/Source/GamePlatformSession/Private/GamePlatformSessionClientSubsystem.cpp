#include "GamePlatformSessionClientSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "State/SessionConnectionState.h"
#include "Types/GamePlatformSessionErrors.h"

#include <limits>
#include <memory>
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSession, Log, All);

namespace
{
constexpr int32 MaxSessionTextChars = 1024;
constexpr int32 MaxEndpointChars = 2048;
constexpr int32 MaxTransferTicketChars = 8192;
constexpr double MinTimeoutSeconds = 1.0;
constexpr double MaxTimeoutSeconds = 120.0;
constexpr float DeadlineTickerIntervalSeconds = 0.1f;
constexpr uint8 AdmissionFactBit = 1u << static_cast<uint8>(EGamePlatformSessionTransferFact::AdmissionConfirmed);

std::string ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), static_cast<size_t>(Converted.Length()));
}

GamePlatformSession::FBinding ToCoreBinding(const FGamePlatformSessionConnectionBinding& Binding)
{
    GamePlatformSession::FBinding Result;
    Result.AssignmentId = ToUtf8(Binding.AssignmentId);
    Result.GameSessionId = ToUtf8(Binding.GameSessionId);
    Result.ServerInstanceId = ToUtf8(Binding.ServerInstanceId);
    Result.ServerBootId = ToUtf8(Binding.ServerBootId);
    Result.WorldId = ToUtf8(Binding.WorldId.ToString());
    Result.ProtocolVersion = ToUtf8(Binding.ProtocolVersion);
    Result.SessionEpoch = Binding.SessionEpoch > 0
        ? static_cast<std::uint64_t>(Binding.SessionEpoch)
        : 0;
    return Result;
}

bool TryToCoreFact(
    EGamePlatformSessionTransferFact Fact,
    GamePlatformSession::EFact& OutFact)
{
    switch (Fact)
    {
    case EGamePlatformSessionTransferFact::NetworkConnected:
        OutFact = GamePlatformSession::EFact::NetworkConnected;
        return true;
    case EGamePlatformSessionTransferFact::AdmissionConfirmed:
        OutFact = GamePlatformSession::EFact::AdmissionConfirmed;
        return true;
    case EGamePlatformSessionTransferFact::TargetWorldLoaded:
        OutFact = GamePlatformSession::EFact::TargetWorldLoaded;
        return true;
    case EGamePlatformSessionTransferFact::ControllerReady:
        OutFact = GamePlatformSession::EFact::ControllerReady;
        return true;
    default:
        return false;
    }
}

bool IsValidTransferFact(EGamePlatformSessionTransferFact Fact)
{
    GamePlatformSession::EFact Ignored;
    return TryToCoreFact(Fact, Ignored);
}

uint8 FactBit(EGamePlatformSessionTransferFact Fact)
{
    return IsValidTransferFact(Fact)
        ? static_cast<uint8>(1u << static_cast<uint8>(Fact))
        : 0u;
}

bool TryToCoreIntent(
    EGamePlatformSessionIntent Intent,
    GamePlatformSession::EIntent& OutIntent)
{
    switch (Intent)
    {
    case EGamePlatformSessionIntent::Join:
        OutIntent = GamePlatformSession::EIntent::Join;
        return true;
    case EGamePlatformSessionIntent::Transfer:
        OutIntent = GamePlatformSession::EIntent::Transfer;
        return true;
    case EGamePlatformSessionIntent::Reconnect:
        OutIntent = GamePlatformSession::EIntent::Reconnect;
        return true;
    default:
        return false;
    }
}

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

bool IsSameSnapshot(
    const FGamePlatformSessionSnapshot& Left,
    const FGamePlatformSessionSnapshot& Right)
{
    return Left.State == Right.State &&
        Left.Intent == Right.Intent &&
        Left.RecoveryState == Right.RecoveryState &&
        Left.bRecoveryRequired == Right.bRecoveryRequired &&
        Left.bCanRetry == Right.bCanRetry &&
        Left.TransferOperationId == Right.TransferOperationId &&
        IsSameBinding(Left.Binding, Right.Binding) &&
        Left.bAdmissionConfirmed == Right.bAdmissionConfirmed &&
        Left.ErrorCode == Right.ErrorCode &&
        Left.ErrorMessage == Right.ErrorMessage;
}

EGamePlatformSessionTransferState MapCoreState(
    const GamePlatformSession::FSnapshot& Core,
    uint8 FactMask)
{
    if (Core.bOperationActive)
    {
        if ((FactMask & AdmissionFactBit) != 0)
        {
            return EGamePlatformSessionTransferState::Admitted;
        }
        switch (Core.State)
        {
        case GamePlatformSession::EState::RequestingAssignment:
            return EGamePlatformSessionTransferState::RequestingAssignment;
        case GamePlatformSession::EState::PreparingConnection:
            return EGamePlatformSessionTransferState::PreparingConnection;
        case GamePlatformSession::EState::Connecting:
            return EGamePlatformSessionTransferState::Connecting;
        case GamePlatformSession::EState::AwaitingAdmission:
            return EGamePlatformSessionTransferState::AwaitingAdmission;
        case GamePlatformSession::EState::Reconnecting:
            return EGamePlatformSessionTransferState::Reconnecting;
        case GamePlatformSession::EState::Transferring:
            return EGamePlatformSessionTransferState::Transferring;
        default:
            return EGamePlatformSessionTransferState::Connecting;
        }
    }

    switch (Core.LastOutcome)
    {
    case GamePlatformSession::EOutcome::Succeeded:
        return Core.Current.IsValid()
            ? EGamePlatformSessionTransferState::Ready
            : EGamePlatformSessionTransferState::Idle;
    case GamePlatformSession::EOutcome::Cancelled:
        return EGamePlatformSessionTransferState::Cancelled;
    case GamePlatformSession::EOutcome::TimedOut:
        return EGamePlatformSessionTransferState::TimedOut;
    case GamePlatformSession::EOutcome::Uncertain:
        return EGamePlatformSessionTransferState::Uncertain;
    case GamePlatformSession::EOutcome::Failed:
    case GamePlatformSession::EOutcome::AuthChanged:
        return EGamePlatformSessionTransferState::Failed;
    default:
        return Core.Current.IsValid()
            ? EGamePlatformSessionTransferState::Ready
            : EGamePlatformSessionTransferState::Idle;
    }
}

EGamePlatformSessionIntent MapCoreIntent(GamePlatformSession::EIntent Intent)
{
    switch (Intent)
    {
    case GamePlatformSession::EIntent::Transfer:
        return EGamePlatformSessionIntent::Transfer;
    case GamePlatformSession::EIntent::Reconnect:
        return EGamePlatformSessionIntent::Reconnect;
    case GamePlatformSession::EIntent::Join:
    default:
        return EGamePlatformSessionIntent::Join;
    }
}

EGamePlatformSessionRecoveryState MapCoreRecovery(GamePlatformSession::ERecovery Recovery)
{
    switch (Recovery)
    {
    case GamePlatformSession::ERecovery::RetryAllowed:
        return EGamePlatformSessionRecoveryState::RetryAllowed;
    case GamePlatformSession::ERecovery::ReconciliationRequired:
        return EGamePlatformSessionRecoveryState::ReconciliationRequired;
    case GamePlatformSession::ERecovery::ReauthenticationRequired:
        return EGamePlatformSessionRecoveryState::ReauthenticationRequired;
    case GamePlatformSession::ERecovery::None:
    default:
        return EGamePlatformSessionRecoveryState::None;
    }
}

bool IsSafeEndpoint(const FString& Endpoint)
{
    if (Endpoint.IsEmpty() || Endpoint.Len() > MaxEndpointChars ||
        Endpoint.Contains(TEXT("\r")) || Endpoint.Contains(TEXT("\n")) ||
        Endpoint.Contains(TEXT("?")) || Endpoint.Contains(TEXT("#")) ||
        Endpoint.Contains(TEXT("/")) || Endpoint.Contains(TEXT("\\")) ||
        Endpoint.Contains(TEXT("@")))
    {
        return false;
    }
    for (const TCHAR Character : Endpoint)
    {
        if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('-') ||
            Character == TEXT('_') || Character == TEXT(':') || Character == TEXT('[') ||
            Character == TEXT(']')))
        {
            return false;
        }
    }
    return true;
}

FName DefaultOutcomeError(GamePlatformSession::EOutcome Outcome)
{
    switch (Outcome)
    {
    case GamePlatformSession::EOutcome::Cancelled:
        return GamePlatformSessionErrors::Cancelled;
    case GamePlatformSession::EOutcome::TimedOut:
        return GamePlatformSessionErrors::TimedOut;
    case GamePlatformSession::EOutcome::Uncertain:
        return GamePlatformSessionErrors::OutcomeUncertain;
    case GamePlatformSession::EOutcome::AuthChanged:
        return GamePlatformSessionErrors::AuthChanged;
    case GamePlatformSession::EOutcome::Failed:
        return GamePlatformSessionErrors::Failed;
    default:
        return NAME_None;
    }
}
}

bool FGamePlatformSessionConnectionBinding::IsValid() const
{
    return !AssignmentId.IsEmpty() &&
        !GameSessionId.IsEmpty() &&
        !ServerInstanceId.IsEmpty() &&
        !ServerBootId.IsEmpty() &&
        !WorldId.IsNone() &&
        !ProtocolVersion.IsEmpty() &&
        SessionEpoch > 0 &&
        AssignmentId.Len() <= MaxSessionTextChars &&
        GameSessionId.Len() <= MaxSessionTextChars &&
        ServerInstanceId.Len() <= MaxSessionTextChars &&
        ServerBootId.Len() <= MaxSessionTextChars &&
        ProtocolVersion.Len() <= MaxSessionTextChars;
}

FGamePlatformResult FGamePlatformSessionTransferRequest::Validate() const
{
    if (!TransferOperationId.IsValid())
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::OperationIdInvalid,
            TEXT("会话转移操作必须具有有效的操作身份。"));
    }
    if (AssignmentId.IsEmpty() || GameServerId.IsEmpty() || ServerRoleId.IsNone() ||
        ExperienceId.IsNone() || WorldId.IsNone() || Endpoint.IsEmpty() ||
        TransferTicket.IsEmpty() || SessionId.IsEmpty() || !ExpectedBinding.IsValid())
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransferRequestIncomplete,
            TEXT("会话转移请求缺少分配、服务器、体验、世界、连接或玩家身份。"));
    }
    if (ExpectedBinding.AssignmentId != AssignmentId ||
        ExpectedBinding.ServerInstanceId != GameServerId ||
        ExpectedBinding.WorldId != WorldId)
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::BindingRejected,
            TEXT("会话转移请求中的权威Binding与Assignment、GameServer或World身份不一致。"));
    }
    if (!IsSafeEndpoint(Endpoint))
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransferEndpointInvalid,
            TEXT("会话连接地址格式无效或包含不允许的URL/控制字符。"));
    }
    if (AssignmentId.Len() > MaxSessionTextChars ||
        GameServerId.Len() > MaxSessionTextChars ||
        Endpoint.Len() > MaxEndpointChars ||
        TransferTicket.Len() > MaxTransferTicketChars ||
        TicketId.Len() > MaxSessionTextChars ||
        CharacterId.Len() > MaxSessionTextChars ||
        SessionId.Len() > MaxSessionTextChars ||
        ExpectedBinding.GameSessionId.Len() > MaxSessionTextChars ||
        ExpectedBinding.ServerBootId.Len() > MaxSessionTextChars ||
        ExpectedBinding.ProtocolVersion.Len() > MaxSessionTextChars)
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransferRequestTooLarge,
            TEXT("会话转移请求字段超过安全长度限制。"));
    }
    if (!FMath::IsFinite(TimeoutSeconds) ||
        TimeoutSeconds < MinTimeoutSeconds ||
        TimeoutSeconds > MaxTimeoutSeconds)
    {
        return FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransferTimeoutInvalid,
            TEXT("会话转移超时预算必须位于1至120秒之间。"));
    }
    return FGamePlatformResult::Success();
}

struct UGamePlatformSessionClientSubsystem::FRuntime
{
    std::unique_ptr<GamePlatformSession::FSessionConnectionState> State;
    GamePlatformSession::FOperationIdentity ActiveIdentity;
    GamePlatformSession::FBinding PreparedBinding;
    FGamePlatformSessionConnectionBinding PublicBinding;
    FGuid ActiveOperationId;
    FGuid LastOperationId;
    FGuid AuthGeneration;
    FString ActiveAttemptId;
    FString AccountId;
    uint64 AuthSerial = 0;
    uint8 FactMask = 0;
    FTSTicker::FDelegateHandle TickerHandle;
};

UGamePlatformSessionClientSubsystem::UGamePlatformSessionClientSubsystem() = default;
UGamePlatformSessionClientSubsystem::UGamePlatformSessionClientSubsystem(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformSessionClientSubsystem::~UGamePlatformSessionClientSubsystem() = default;

void UGamePlatformSessionClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Runtime = MakeUnique<FRuntime>();
    Runtime->State = std::make_unique<GamePlatformSession::FSessionConnectionState>(
        ToUtf8(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)));
    Snapshot = {};
}

void UGamePlatformSessionClientSubsystem::Deinitialize()
{
    StopTicker();
    if (Transport.IsValid() && Runtime)
    {
        if (Runtime->ActiveOperationId.IsValid())
        {
            Transport->CancelTransfer(Runtime->ActiveOperationId);
        }
        if (Runtime->PublicBinding.IsValid())
        {
            Transport->LeaveSession(Runtime->PublicBinding);
        }
    }
    Transport.Reset();
    Runtime.Reset();
    Snapshot = {};
    SessionChanged.Clear();
    Super::Deinitialize();
}

void UGamePlatformSessionClientSubsystem::SetTransport(
    TSharedPtr<IGamePlatformSessionTransport> InTransport)
{
    check(IsInGameThread());
    if (Transport == InTransport)
    {
        return;
    }

    FGamePlatformResult Ignored;
    if (Runtime && Runtime->PublicBinding.IsValid())
    {
        LeaveSession(Ignored);
    }
    else
    {
        CancelTransfer(Ignored);
    }
    Transport = MoveTemp(InTransport);
}

void UGamePlatformSessionClientSubsystem::SetAuthenticationContext(
    const FString& AccountId,
    const FGuid& AuthGeneration)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State)
    {
        return;
    }
    if (Runtime->AccountId == AccountId && Runtime->AuthGeneration == AuthGeneration)
    {
        return;
    }

    const auto CoreBefore = Runtime->State->Snapshot();
    if (Transport.IsValid())
    {
        if (Runtime->ActiveOperationId.IsValid())
        {
            Transport->CancelTransfer(Runtime->ActiveOperationId);
        }
        if (Runtime->PublicBinding.IsValid())
        {
            // 认证上下文改变前先断开旧账号的真实网络，避免本地已登出但仍连接旧服务器。
            Transport->LeaveSession(Runtime->PublicBinding);
        }
    }
    StopTicker();
    if (CoreBefore.bOperationActive || CoreBefore.Current.IsValid())
    {
        Runtime->State->Leave();
    }

    Runtime->AccountId = AccountId;
    Runtime->AuthGeneration = AuthGeneration;
    if (Runtime->AuthSerial == std::numeric_limits<uint64>::max())
    {
        // 极端代次耗尽时 Fail Closed；不回绕后接受可能陈旧的认证上下文。
        Snapshot.State = EGamePlatformSessionTransferState::Failed;
        Snapshot.ErrorCode = GamePlatformSessionErrors::AuthGenerationExhausted;
        Snapshot.ErrorMessage = TEXT("会话认证代次已经耗尽，必须重建GameInstance。");
        SessionChanged.Broadcast(Snapshot);
        return;
    }
    ++Runtime->AuthSerial;

    Runtime->State->SetAuthentication(ToUtf8(AccountId), Runtime->AuthSerial);
    Runtime->ActiveIdentity = {};
    Runtime->PreparedBinding = {};
    Runtime->PublicBinding = {};
    Runtime->ActiveOperationId.Invalidate();
    Runtime->LastOperationId.Invalidate();
    Runtime->ActiveAttemptId.Reset();
    Runtime->FactMask = 0;
    RefreshSnapshot();
}

bool UGamePlatformSessionClientSubsystem::BeginTransfer(
    const FGamePlatformSessionTransferRequest& Request,
    FGamePlatformResult& OutResult)
{
    const bool bHasCurrent = Runtime && Runtime->State &&
        Runtime->State->Snapshot().Current.IsValid();
    return BeginOperation(
        bHasCurrent ? EGamePlatformSessionIntent::Transfer : EGamePlatformSessionIntent::Join,
        Request,
        OutResult);
}

bool UGamePlatformSessionClientSubsystem::Reconnect(
    const FGamePlatformSessionTransferRequest& Request,
    FGamePlatformResult& OutResult)
{
    return BeginOperation(EGamePlatformSessionIntent::Reconnect, Request, OutResult);
}

bool UGamePlatformSessionClientSubsystem::BeginOperation(
    EGamePlatformSessionIntent Intent,
    const FGamePlatformSessionTransferRequest& Request,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    OutResult = Request.Validate();
    if (!OutResult.IsSuccess())
    {
        return false;
    }
    if (!Runtime || !Runtime->State)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::Unavailable,
            TEXT("平台会话子系统尚未初始化。"));
        return false;
    }
    if (!Transport.IsValid())
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransportUnavailable,
            TEXT("没有安装真实会话传输适配器，拒绝伪造服务器连接成功。"));
        RefreshSnapshot(OutResult.Code, OutResult.Message);
        return false;
    }

    GamePlatformSession::EIntent CoreIntent;
    UE_LOG(
        LogGamePlatformSession,
        Verbose,
        TEXT("Session operation begin. Operation=%s Assignment=%s Intent=%d"),
        *Request.TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
        *Request.AssignmentId,
        static_cast<int32>(Intent));
    if (!TryToCoreIntent(Intent, CoreIntent))
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::IntentInvalid,
            TEXT("会话操作意图无效。"));
        return false;
    }

    const GamePlatformSession::FSnapshot CoreBefore = Runtime->State->Snapshot();
    if (CoreBefore.bOperationActive &&
        Runtime->ActiveOperationId == Request.TransferOperationId &&
        CoreBefore.ActiveIntent == CoreIntent)
    {
        // 同一幂等键的活动重入只返回当前接纳结果，绝不再次调用Transport或重复ClientTravel。
        OutResult = FGamePlatformResult::Success();
        return true;
    }
    if (!CoreBefore.bOperationActive &&
        Runtime->LastOperationId == Request.TransferOperationId)
    {
        if (CoreBefore.LastOutcome == GamePlatformSession::EOutcome::Succeeded)
        {
            OutResult = FGamePlatformResult::Success();
            return true;
        }
        const FName PreviousError = DefaultOutcomeError(CoreBefore.LastOutcome);
        OutResult = FGamePlatformResult::Failure(
            PreviousError.IsNone() ? GamePlatformSessionErrors::TransferRejected : PreviousError,
            TEXT("该会话操作身份已经结束；重试必须使用新的TransferOperationId。"));
        return false;
    }

    GamePlatformSession::FOperationIdentity Identity;
    const double NowSeconds = FPlatformTime::Seconds();
    const FString AttemptId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    const auto Acceptance = Runtime->State->Begin(
        CoreIntent,
        ToUtf8(Request.TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower)),
        ToUtf8(AttemptId),
        NowSeconds,
        NowSeconds + Request.TimeoutSeconds,
        Identity);
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::TransferRejected,
            TEXT("当前会话状态拒绝新的操作；可能存在活动操作、未认证或必须先完成远端对账。"));
        RefreshSnapshot(OutResult.Code, OutResult.Message);
        return false;
    }

    Runtime->ActiveIdentity = Identity;
    Runtime->ActiveOperationId = Request.TransferOperationId;
    Runtime->LastOperationId = Request.TransferOperationId;
    Runtime->ActiveAttemptId = AttemptId;
    Runtime->PreparedBinding = {};
    Runtime->PublicBinding = {};
    Runtime->FactMask = 0;
    Snapshot.TransferOperationId = Request.TransferOperationId;
    Snapshot.ErrorCode = NAME_None;
    Snapshot.ErrorMessage.Reset();
    RefreshSnapshot();

    UE_LOG(
        LogGamePlatformSession,
        Log,
        TEXT("Session operation accepted. Operation=%s Assignment=%s Server=%s Intent=%d Timeout=%.2fs"),
        *Request.TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
        *Request.AssignmentId,
        *Request.GameServerId,
        static_cast<int32>(Intent),
        Request.TimeoutSeconds);

    if (!Runtime->TickerHandle.IsValid())
    {
        Runtime->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this,
                &UGamePlatformSessionClientSubsystem::TickActiveOperation),
            DeadlineTickerIntervalSeconds);
    }

    const FGuid ExpectedOperation = Request.TransferOperationId;
    FGamePlatformSessionTransportCallbacks Callbacks;
    Callbacks.OnBindingPrepared =
        [WeakThis = TWeakObjectPtr<UGamePlatformSessionClientSubsystem>(this), ExpectedOperation](
            FGamePlatformSessionConnectionBinding Binding)
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, ExpectedOperation, Binding = MoveTemp(Binding)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->HandleBindingPrepared(ExpectedOperation, MoveTemp(Binding));
                    }
                });
        };
    Callbacks.OnTravelCommitted =
        [WeakThis = TWeakObjectPtr<UGamePlatformSessionClientSubsystem>(this), ExpectedOperation](
            FGamePlatformSessionConnectionBinding Binding)
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, ExpectedOperation, Binding = MoveTemp(Binding)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->HandleTravelCommitted(ExpectedOperation, MoveTemp(Binding));
                    }
                });
        };
    Callbacks.OnFact =
        [WeakThis = TWeakObjectPtr<UGamePlatformSessionClientSubsystem>(this), ExpectedOperation](
            EGamePlatformSessionTransferFact Fact,
            FGamePlatformSessionConnectionBinding Binding)
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, ExpectedOperation, Fact, Binding = MoveTemp(Binding)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->HandleTransportFact(
                            ExpectedOperation,
                            Fact,
                            MoveTemp(Binding));
                    }
                });
        };
    Callbacks.OnFailed =
        [WeakThis = TWeakObjectPtr<UGamePlatformSessionClientSubsystem>(this), ExpectedOperation](
            FName ErrorCode,
            FString ErrorMessage)
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, ExpectedOperation, ErrorCode, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->HandleTransportFailure(
                            ExpectedOperation,
                            ErrorCode,
                            MoveTemp(ErrorMessage));
                    }
                });
        };

    // TransferTicket 仅在这一调用边界传给真实传输层；Subsystem自身不复制、不持久化、不记录原文。
    Transport->BeginTransfer(Request, MoveTemp(Callbacks));
    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSessionClientSubsystem::CancelTransfer(FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State || !Runtime->ActiveOperationId.IsValid())
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::NoActiveTransfer,
            TEXT("当前没有可取消的会话转移操作。"));
        return false;
    }

    const FGuid OperationId = Runtime->ActiveOperationId;
    const auto Acceptance = Runtime->State->Cancel(
        Runtime->ActiveIdentity,
        FPlatformTime::Seconds());
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::CancelRejected,
            TEXT("活动会话操作已经结束或身份代次过期。"));
        return false;
    }

    if (Transport.IsValid())
    {
        Transport->CancelTransfer(OperationId);
    }
    StopTicker();
    Runtime->LastOperationId = OperationId;
    Runtime->ActiveOperationId.Invalidate();
    Runtime->ActiveAttemptId.Reset();
    Runtime->FactMask = 0;
    RefreshSnapshot();
    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSessionClientSubsystem::LeaveSession(FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::Unavailable,
            TEXT("平台会话子系统尚未初始化。"));
        return false;
    }

    const FGuid OperationId = Runtime->ActiveOperationId.IsValid()
        ? Runtime->ActiveOperationId
        : Runtime->LastOperationId;
    const FGamePlatformSessionConnectionBinding Binding = Runtime->PublicBinding;
    if (Transport.IsValid())
    {
        if (Runtime->ActiveOperationId.IsValid())
        {
            Transport->CancelTransfer(Runtime->ActiveOperationId);
        }
        if (Binding.IsValid())
        {
            Transport->LeaveSession(Binding);
        }
    }

    Runtime->State->Leave();
    StopTicker();
    Runtime->LastOperationId = OperationId;
    Runtime->ActiveOperationId.Invalidate();
    Runtime->ActiveAttemptId.Reset();
    Runtime->PreparedBinding = {};
    Runtime->PublicBinding = {};
    Runtime->FactMask = 0;
    RefreshSnapshot();
    UE_LOG(
        LogGamePlatformSession,
        Log,
        TEXT("Session leave requested. Operation=%s HadBinding=%d"),
        *OperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
        Binding.IsValid() ? 1 : 0);
    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSessionClientSubsystem::NotifyDisconnected(
    const FGamePlatformSessionConnectionBinding& Binding,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State || !Binding.IsValid() ||
        !Runtime->State->Disconnect(ToCoreBinding(Binding)))
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::DisconnectRejected,
            TEXT("断线通知不属于当前已建立会话，旧连接不会清理新绑定。"));
        return false;
    }

    Runtime->PublicBinding = {};
    Runtime->PreparedBinding = {};
    Runtime->FactMask = 0;
    RefreshSnapshot();
    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSessionClientSubsystem::ResolveRemoteState(
    const FGuid& TransferOperationId,
    const FGamePlatformSessionConnectionBinding& ConfirmedBinding,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State || !TransferOperationId.IsValid() ||
        Runtime->LastOperationId != TransferOperationId)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::StaleOperation,
            TEXT("远端对账结果不属于最近一次会话操作。"));
        return false;
    }

    const GamePlatformSession::FBinding CoreBinding = ConfirmedBinding.IsValid()
        ? ToCoreBinding(ConfirmedBinding)
        : GamePlatformSession::FBinding();
    const auto Acceptance = Runtime->State->ResolveRemote(Runtime->ActiveIdentity, CoreBinding);
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::ReconciliationRejected,
            TEXT("远端会话对账结果与当前操作身份或本地连接绑定不一致。"));
        return false;
    }

    if (!ConfirmedBinding.IsValid())
    {
        Runtime->PublicBinding = {};
        Runtime->PreparedBinding = {};
    }
    RefreshSnapshot();
    UE_LOG(
        LogGamePlatformSession,
        Log,
        TEXT("Session reconciliation completed. Operation=%s BindingPresent=%d"),
        *TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
        ConfirmedBinding.IsValid() ? 1 : 0);
    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSessionClientSubsystem::ReportLocalFact(
    const FGuid& TransferOperationId,
    EGamePlatformSessionTransferFact Fact,
    const FGamePlatformSessionConnectionBinding& Binding,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!IsValidTransferFact(Fact))
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::FactInvalid,
            TEXT("会话事实枚举值无效，拒绝推进状态。"));
        return false;
    }
    if (Fact == EGamePlatformSessionTransferFact::NetworkConnected ||
        Fact == EGamePlatformSessionTransferFact::AdmissionConfirmed)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::AuthoritativeFactRequired,
            TEXT("网络连接和服务器准入事实只能由真实Session Transport报告。"));
        return false;
    }
    if (!Runtime || Runtime->ActiveOperationId != TransferOperationId)
    {
        OutResult = FGamePlatformResult::Failure(
            GamePlatformSessionErrors::StaleOperation,
            TEXT("本地会话事实属于旧操作或当前没有活动转移。"));
        return false;
    }

    HandleTransportFact(TransferOperationId, Fact, Binding);
    const bool bAccepted = (Runtime->FactMask & FactBit(Fact)) != 0;
    OutResult = bAccepted
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(
            GamePlatformSessionErrors::FactRejected,
            TEXT("会话事实未通过当前操作身份或连接绑定校验。"));
    return bAccepted;
}

bool UGamePlatformSessionClientSubsystem::TickActiveOperation(float DeltaSeconds)
{
    if (!Runtime || !Runtime->State)
    {
        return false;
    }

    const FGuid OperationId = Runtime->ActiveOperationId;
    Runtime->State->AdvanceDeadline(FPlatformTime::Seconds());
    const auto Core = Runtime->State->Snapshot();
    RefreshSnapshot();
    if (!Core.bOperationActive)
    {
        // Deadline终止本地操作时同步通知Transport停止仍在进行的连接动作；旅行后状态会保持Uncertain等待对账。
        UE_LOG(
            LogGamePlatformSession,
            Warning,
            TEXT("Session operation deadline reached. Operation=%s Outcome=%d Recovery=%d"),
            *OperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
            static_cast<int32>(Core.LastOutcome),
            static_cast<int32>(Core.Recovery));
        if (Transport.IsValid() && OperationId.IsValid())
        {
            Transport->CancelTransfer(OperationId);
        }
        Runtime->TickerHandle.Reset();
        Runtime->LastOperationId = OperationId;
        Runtime->ActiveOperationId.Invalidate();
        Runtime->ActiveAttemptId.Reset();
        Runtime->FactMask = 0;
        return false;
    }
    return true;
}

void UGamePlatformSessionClientSubsystem::HandleBindingPrepared(
    const FGuid& TransferOperationId,
    FGamePlatformSessionConnectionBinding Binding)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State ||
        Runtime->ActiveOperationId != TransferOperationId ||
        !Binding.IsValid())
    {
        return;
    }

    const GamePlatformSession::FBinding CoreBinding = ToCoreBinding(Binding);
    if (Runtime->State->Assign(
            Runtime->ActiveIdentity,
            CoreBinding,
            FPlatformTime::Seconds()) != GamePlatformSession::EAcceptance::Accepted)
    {
        HandleTransportFailure(
            TransferOperationId,
            GamePlatformSessionErrors::BindingRejected,
            TEXT("服务器连接绑定与当前会话操作不一致。"));
        return;
    }

    Runtime->PreparedBinding = CoreBinding;
    Runtime->PublicBinding = MoveTemp(Binding);
    RefreshSnapshot();
}

void UGamePlatformSessionClientSubsystem::HandleTravelCommitted(
    const FGuid& TransferOperationId,
    FGamePlatformSessionConnectionBinding Binding)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State ||
        Runtime->ActiveOperationId != TransferOperationId ||
        !Binding.IsValid() ||
        !(ToCoreBinding(Binding) == Runtime->PreparedBinding))
    {
        return;
    }

    if (Runtime->State->CommitTravel(
            Runtime->ActiveIdentity,
            FPlatformTime::Seconds()) != GamePlatformSession::EAcceptance::Accepted)
    {
        HandleTransportFailure(
            TransferOperationId,
            GamePlatformSessionErrors::TravelCommitRejected,
            TEXT("真实旅行边界与当前会话绑定不一致。"));
        return;
    }
    RefreshSnapshot();
}

void UGamePlatformSessionClientSubsystem::HandleTransportFact(
    const FGuid& TransferOperationId,
    EGamePlatformSessionTransferFact Fact,
    FGamePlatformSessionConnectionBinding Binding)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State ||
        Runtime->ActiveOperationId != TransferOperationId ||
        !Binding.IsValid() ||
        !(ToCoreBinding(Binding) == Runtime->PreparedBinding))
    {
        return;
    }

    GamePlatformSession::EFact CoreFact;
    if (!TryToCoreFact(Fact, CoreFact))
    {
        return;
    }
    const auto Acceptance = Runtime->State->Observe(
        Runtime->ActiveIdentity,
        Runtime->PreparedBinding,
        CoreFact,
        FPlatformTime::Seconds());
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        return;
    }

    Runtime->FactMask |= FactBit(Fact);
    Runtime->PublicBinding = MoveTemp(Binding);
    RefreshSnapshot();

    const auto Core = Runtime->State->Snapshot();
    if (!Core.bOperationActive)
    {
        StopTicker();
        Runtime->LastOperationId = TransferOperationId;
        Runtime->ActiveOperationId.Invalidate();
        Runtime->ActiveAttemptId.Reset();
        Runtime->FactMask = 0;
    }
}

void UGamePlatformSessionClientSubsystem::HandleTransportFailure(
    const FGuid& TransferOperationId,
    FName ErrorCode,
    FString ErrorMessage)
{
    check(IsInGameThread());
    if (!Runtime || !Runtime->State ||
        Runtime->ActiveOperationId != TransferOperationId)
    {
        return;
    }

    Runtime->State->Fail(
        Runtime->ActiveIdentity,
        FPlatformTime::Seconds());
    StopTicker();
    Runtime->LastOperationId = TransferOperationId;
    Runtime->ActiveOperationId.Invalidate();
    Runtime->ActiveAttemptId.Reset();
    Runtime->FactMask = 0;
    RefreshSnapshot(
        ErrorCode.IsNone() ? GamePlatformSessionErrors::TransportFailed : ErrorCode,
        ErrorMessage.IsEmpty()
            ? FString(TEXT("真实会话传输失败。"))
            : MoveTemp(ErrorMessage));
}

void UGamePlatformSessionClientSubsystem::RefreshSnapshot(
    FName ErrorCode,
    FString ErrorMessage)
{
    if (!Runtime || !Runtime->State)
    {
        return;
    }

    const auto Core = Runtime->State->Snapshot();
    FGamePlatformSessionSnapshot Next;
    Next.State = MapCoreState(Core, Runtime->FactMask);
    Next.Intent = MapCoreIntent(Core.ActiveIntent);
    Next.RecoveryState = MapCoreRecovery(Core.Recovery);
    Next.bRecoveryRequired = Core.bRemoteResolutionRequired ||
        Core.Recovery == GamePlatformSession::ERecovery::ReconciliationRequired ||
        Core.Recovery == GamePlatformSession::ERecovery::ReauthenticationRequired;
    Next.bCanRetry = !Core.bOperationActive && !Core.bRemoteResolutionRequired &&
        !Runtime->AccountId.IsEmpty() &&
        Core.Recovery != GamePlatformSession::ERecovery::ReauthenticationRequired;
    Next.TransferOperationId = Runtime->ActiveOperationId.IsValid()
        ? Runtime->ActiveOperationId
        : Runtime->LastOperationId;
    Next.Binding = Runtime->PublicBinding;
    Next.bAdmissionConfirmed =
        (Runtime->FactMask & AdmissionFactBit) != 0 ||
        Next.State == EGamePlatformSessionTransferState::Ready;

    if (!ErrorCode.IsNone())
    {
        // 显式适配器/结构错误必须直接呈现为失败，不能因为纯状态内核仍是Idle而被UI误判为可用。
        Next.State = EGamePlatformSessionTransferState::Failed;
        Next.ErrorCode = ErrorCode;
        Next.ErrorMessage = MoveTemp(ErrorMessage);
    }
    else
    {
        Next.ErrorCode = DefaultOutcomeError(Core.LastOutcome);
        if (!Next.ErrorCode.IsNone())
        {
            Next.ErrorMessage = TEXT("会话操作以非成功终态结束；请结合错误码执行恢复或重新分配。");
        }
    }

    if (IsSameSnapshot(Snapshot, Next))
    {
        return;
    }

    UE_LOG(
        LogGamePlatformSession,
        Verbose,
        TEXT("Session snapshot changed. Operation=%s State=%d Intent=%d Recovery=%d Admission=%d Error=%s Assignment=%s Server=%s Epoch=%lld"),
        *Next.TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower),
        static_cast<int32>(Next.State),
        static_cast<int32>(Next.Intent),
        static_cast<int32>(Next.RecoveryState),
        Next.bAdmissionConfirmed ? 1 : 0,
        *Next.ErrorCode.ToString(),
        *Next.Binding.AssignmentId,
        *Next.Binding.ServerInstanceId,
        Next.Binding.SessionEpoch);

    Snapshot = MoveTemp(Next);
    SessionChanged.Broadcast(Snapshot);
}

void UGamePlatformSessionClientSubsystem::StopTicker()
{
    if (Runtime && Runtime->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Runtime->TickerHandle);
        Runtime->TickerHandle.Reset();
    }
}
