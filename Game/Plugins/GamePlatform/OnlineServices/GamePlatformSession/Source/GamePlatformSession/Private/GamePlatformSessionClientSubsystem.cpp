#include "GamePlatformSessionClientSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "State/SessionConnectionState.h"

#include <limits>
#include <memory>
#include <string>

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

GamePlatformSession::EFact ToCoreFact(EGamePlatformSessionTransferFact Fact)
{
    switch (Fact)
    {
    case EGamePlatformSessionTransferFact::NetworkConnected:
        return GamePlatformSession::EFact::NetworkConnected;
    case EGamePlatformSessionTransferFact::AdmissionConfirmed:
        return GamePlatformSession::EFact::AdmissionConfirmed;
    case EGamePlatformSessionTransferFact::TargetWorldLoaded:
        return GamePlatformSession::EFact::TargetWorldLoaded;
    case EGamePlatformSessionTransferFact::ControllerReady:
    default:
        return GamePlatformSession::EFact::ControllerReady;
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
            return EGamePlatformSessionTransferState::PreparingConnection;
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

FName DefaultOutcomeError(GamePlatformSession::EOutcome Outcome)
{
    switch (Outcome)
    {
    case GamePlatformSession::EOutcome::Cancelled:
        return TEXT("SessionCancelled");
    case GamePlatformSession::EOutcome::TimedOut:
        return TEXT("SessionTimedOut");
    case GamePlatformSession::EOutcome::Uncertain:
        return TEXT("SessionOutcomeUncertain");
    case GamePlatformSession::EOutcome::AuthChanged:
        return TEXT("SessionAuthChanged");
    case GamePlatformSession::EOutcome::Failed:
        return TEXT("SessionFailed");
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
            TEXT("SessionOperationIdInvalid"),
            TEXT("会话转移操作必须具有有效的操作身份。"));
    }
    if (AssignmentId.IsEmpty() || GameServerId.IsEmpty() || ServerRoleId.IsNone() ||
        ExperienceId.IsNone() || WorldId.IsNone() || Endpoint.IsEmpty() ||
        TransferTicket.IsEmpty() || CharacterId.IsEmpty() || SessionId.IsEmpty())
    {
        return FGamePlatformResult::Failure(
            TEXT("SessionTransferRequestIncomplete"),
            TEXT("会话转移请求缺少分配、服务器、体验、世界、连接或玩家身份。"));
    }
    if (AssignmentId.Len() > MaxSessionTextChars ||
        GameServerId.Len() > MaxSessionTextChars ||
        Endpoint.Len() > MaxEndpointChars ||
        TransferTicket.Len() > MaxTransferTicketChars ||
        TicketId.Len() > MaxSessionTextChars ||
        CharacterId.Len() > MaxSessionTextChars ||
        SessionId.Len() > MaxSessionTextChars)
    {
        return FGamePlatformResult::Failure(
            TEXT("SessionTransferRequestTooLarge"),
            TEXT("会话转移请求字段超过安全长度限制。"));
    }
    if (!FMath::IsFinite(TimeoutSeconds) ||
        TimeoutSeconds < MinTimeoutSeconds ||
        TimeoutSeconds > MaxTimeoutSeconds)
    {
        return FGamePlatformResult::Failure(
            TEXT("SessionTransferTimeoutInvalid"),
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
    FGuid AuthGeneration;
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
    if (Transport.IsValid() && Runtime && Runtime->ActiveOperationId.IsValid())
    {
        Transport->CancelTransfer(Runtime->ActiveOperationId);
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
    CancelTransfer(Ignored);
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

    if (Transport.IsValid() && Runtime->ActiveOperationId.IsValid())
    {
        Transport->CancelTransfer(Runtime->ActiveOperationId);
    }
    StopTicker();

    Runtime->AccountId = AccountId;
    Runtime->AuthGeneration = AuthGeneration;
    if (Runtime->AuthSerial == std::numeric_limits<uint64>::max())
    {
        // 极端代次耗尽时 Fail Closed；不回绕后接受可能陈旧的认证上下文。
        Snapshot.State = EGamePlatformSessionTransferState::Failed;
        Snapshot.ErrorCode = TEXT("SessionAuthGenerationExhausted");
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
    Runtime->FactMask = 0;
    RefreshSnapshot();
}

bool UGamePlatformSessionClientSubsystem::BeginTransfer(
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
            TEXT("SessionUnavailable"),
            TEXT("平台会话子系统尚未初始化。"));
        return false;
    }
    if (!Transport.IsValid())
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SessionTransportUnavailable"),
            TEXT("没有安装真实会话传输适配器，拒绝伪造服务器连接成功。"));
        RefreshSnapshot(OutResult.Code, OutResult.Message);
        return false;
    }

    const GamePlatformSession::FSnapshot CoreBefore = Runtime->State->Snapshot();
    const GamePlatformSession::EIntent Intent = CoreBefore.Current.IsValid()
        ? GamePlatformSession::EIntent::Transfer
        : GamePlatformSession::EIntent::Join;

    GamePlatformSession::FOperationIdentity Identity;
    const double NowSeconds = FPlatformTime::Seconds();
    const FString AttemptId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    const auto Acceptance = Runtime->State->Begin(
        Intent,
        ToUtf8(Request.TransferOperationId.ToString(EGuidFormats::DigitsWithHyphensLower)),
        ToUtf8(AttemptId),
        NowSeconds,
        NowSeconds + Request.TimeoutSeconds,
        Identity);
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SessionTransferRejected"),
            TEXT("当前会话状态拒绝新的转移操作；可能存在活动操作、未认证或未解决的远端结果。"));
        RefreshSnapshot(OutResult.Code, OutResult.Message);
        return false;
    }

    Runtime->ActiveIdentity = Identity;
    Runtime->ActiveOperationId = Request.TransferOperationId;
    Runtime->PreparedBinding = {};
    Runtime->PublicBinding = {};
    Runtime->FactMask = 0;
    Snapshot.TransferOperationId = Request.TransferOperationId;
    Snapshot.ErrorCode = NAME_None;
    Snapshot.ErrorMessage.Reset();
    RefreshSnapshot();

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

    // TransferTicket 仅在这一调用边界传给真实传输层；Subsystem 自身不复制或长期保存原文。
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
            TEXT("SessionNoActiveTransfer"),
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
            TEXT("SessionCancelRejected"),
            TEXT("活动会话操作已经结束或身份代次过期。"));
        return false;
    }

    if (Transport.IsValid())
    {
        Transport->CancelTransfer(OperationId);
    }
    StopTicker();
    Runtime->ActiveOperationId.Invalidate();
    Runtime->FactMask = 0;
    RefreshSnapshot();
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
    if (Fact == EGamePlatformSessionTransferFact::NetworkConnected ||
        Fact == EGamePlatformSessionTransferFact::AdmissionConfirmed)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SessionAuthoritativeFactRequired"),
            TEXT("网络连接和服务器准入事实只能由真实Session Transport报告。"));
        return false;
    }
    if (!Runtime || Runtime->ActiveOperationId != TransferOperationId)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SessionStaleOperation"),
            TEXT("本地会话事实属于旧操作或当前没有活动转移。"));
        return false;
    }

    HandleTransportFact(TransferOperationId, Fact, Binding);
    const bool bAccepted = Runtime->FactMask &
        (1u << static_cast<uint8>(Fact));
    OutResult = bAccepted
        ? FGamePlatformResult::Success()
        : FGamePlatformResult::Failure(
            TEXT("SessionFactRejected"),
            TEXT("会话事实未通过当前操作身份或连接绑定校验。"));
    return bAccepted;
}

bool UGamePlatformSessionClientSubsystem::TickActiveOperation(float DeltaSeconds)
{
    if (!Runtime || !Runtime->State)
    {
        return false;
    }

    Runtime->State->AdvanceDeadline(FPlatformTime::Seconds());
    const auto Core = Runtime->State->Snapshot();
    RefreshSnapshot();
    if (!Core.bOperationActive)
    {
        Runtime->TickerHandle.Reset();
        Runtime->ActiveOperationId.Invalidate();
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
            TEXT("SessionBindingRejected"),
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
            TEXT("SessionTravelCommitRejected"),
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

    const auto Acceptance = Runtime->State->Observe(
        Runtime->ActiveIdentity,
        Runtime->PreparedBinding,
        ToCoreFact(Fact),
        FPlatformTime::Seconds());
    if (Acceptance != GamePlatformSession::EAcceptance::Accepted)
    {
        return;
    }

    Runtime->FactMask |= 1u << static_cast<uint8>(Fact);
    Runtime->PublicBinding = MoveTemp(Binding);
    RefreshSnapshot();

    const auto Core = Runtime->State->Snapshot();
    if (!Core.bOperationActive)
    {
        StopTicker();
        Runtime->ActiveOperationId.Invalidate();
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
    Runtime->ActiveOperationId.Invalidate();
    Runtime->FactMask = 0;
    RefreshSnapshot(
        ErrorCode.IsNone() ? FName(TEXT("SessionTransportFailed")) : ErrorCode,
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
    Next.TransferOperationId = Runtime->ActiveOperationId;
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
