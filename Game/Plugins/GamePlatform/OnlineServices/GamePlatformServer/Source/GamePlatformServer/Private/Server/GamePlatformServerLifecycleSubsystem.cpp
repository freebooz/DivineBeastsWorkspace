#include "Server/GamePlatformServerLifecycleSubsystem.h"

#include "Async/Async.h"
#include "Features/IModularFeatures.h"
#include "Engine/Engine.h"

bool FGamePlatformServerInstanceInfo::IsValid() const
{
    const auto IsBoundedValue = [](const FString& Value)
    {
        return !Value.IsEmpty() && Value.Len() <= 256 &&
            !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n"));
    };
    const auto IsOptionalBoundedValue = [&IsBoundedValue](const FString& Value)
    {
        return Value.IsEmpty() || IsBoundedValue(Value);
    };
    return IsBoundedValue(GameId) && IsBoundedValue(GameServerId) &&
        IsBoundedValue(ServerBootId) &&
        IsBoundedValue(ServerRoleId) && IsBoundedValue(ExperienceId) &&
        IsBoundedValue(WorldId) && IsBoundedValue(RegionId) &&
        IsOptionalBoundedValue(ClusterId) && IsOptionalBoundedValue(NodeId) &&
        IsBoundedValue(BuildVersion) && IsBoundedValue(PublicEndpoint) &&
        ProtocolVersion > 0 && Capacity > 0;
}

FName IGamePlatformServerControlProvider::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformServerControlProvider"));
}

bool UGamePlatformServerLifecycleSubsystem::ShouldCreateSubsystem(
    UObject* Outer) const
{
    return IsRunningDedicatedServer() && !IsRunningCommandlet() &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformServerLifecycleSubsystem::Deinitialize()
{
    // 使所有迟到回调失效；关闭期间不发新网络操作。
    bIsClosing = true;
    StopHeartbeatPump();
    CancelControlRetry();
    DeferredControlOperation.Reset();
    ++Generation;
    bHeartbeatInFlight = false;
    bControlOperationInFlight = false;
    LifecycleChanged.Clear();
    ActiveInstance = FGamePlatformServerInstanceInfo();
    Snapshot = FGamePlatformServerLifecycleSnapshot();
    Super::Deinitialize();
}

bool UGamePlatformServerLifecycleSubsystem::RegisterInstance(
    const FGamePlatformServerInstanceInfo& Instance)
{
    check(IsInGameThread());
    if (bIsClosing || Snapshot.State != EGamePlatformServerLifecycleState::Unregistered ||
        !Instance.IsValid())
    {
        return false;
    }
    ActiveInstance = Instance;
    Snapshot.GameServerId = Instance.GameServerId;
    Snapshot.ServerRoleId = Instance.ServerRoleId;
    Snapshot.ExperienceId = Instance.ExperienceId;
    return StartProviderOperation(EControlOperation::Register);
}

bool UGamePlatformServerLifecycleSubsystem::SendHeartbeat(int32 CurrentPlayers)
{
    check(IsInGameThread());
    const bool bCanSend = Snapshot.State == EGamePlatformServerLifecycleState::Registered ||
        Snapshot.State == EGamePlatformServerLifecycleState::Ready ||
        Snapshot.State == EGamePlatformServerLifecycleState::Draining;
    if (bIsClosing || !bCanSend || bHeartbeatInFlight || CurrentPlayers < 0 ||
        CurrentPlayers > ActiveInstance.Capacity)
    {
        return false;
    }
    bHeartbeatInFlight = true;
    return StartProviderOperation(EControlOperation::Heartbeat, CurrentPlayers);
}

bool UGamePlatformServerLifecycleSubsystem::StartHeartbeatPump(
    FGamePlatformServerPlayerCountProvider PlayerCountProvider,
    float IntervalSeconds)
{
    check(IsInGameThread());
    const bool bLifecycleAllowsHeartbeat =
        Snapshot.State == EGamePlatformServerLifecycleState::Registered ||
        Snapshot.State == EGamePlatformServerLifecycleState::Ready ||
        Snapshot.State == EGamePlatformServerLifecycleState::Draining;
    if (bIsClosing || !bLifecycleAllowsHeartbeat || !PlayerCountProvider ||
        !FMath::IsFinite(IntervalSeconds) || IntervalSeconds < 1.0f ||
        IntervalSeconds > 60.0f)
    {
        return false;
    }

    // Ready状态可能因瞬时心跳恢复再次广播；已有泵直接复用，避免重复Ticker和重复HTTP。
    if (HeartbeatTickerHandle.IsValid())
    {
        return true;
    }

    HeartbeatPlayerCountProvider = MoveTemp(PlayerCountProvider);
    HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(
            this,
            &UGamePlatformServerLifecycleSubsystem::TickHeartbeat),
        IntervalSeconds);
    if (!HeartbeatTickerHandle.IsValid())
    {
        HeartbeatPlayerCountProvider = nullptr;
        return false;
    }
    return true;
}

void UGamePlatformServerLifecycleSubsystem::StopHeartbeatPump()
{
    check(IsInGameThread());
    if (HeartbeatTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
        HeartbeatTickerHandle.Reset();
    }
    HeartbeatPlayerCountProvider = nullptr;
}

bool UGamePlatformServerLifecycleSubsystem::MarkReady()
{
    check(IsInGameThread());
    if (bIsClosing || DeferredControlOperation.IsSet()) return false;
    if (Snapshot.State != EGamePlatformServerLifecycleState::Registered)
    {
        return false;
    }
    return StartProviderOperation(EControlOperation::Ready);
}

bool UGamePlatformServerLifecycleSubsystem::BeginDrain()
{
    check(IsInGameThread());
    if (bIsClosing) return false;
    if (Snapshot.State != EGamePlatformServerLifecycleState::Registered &&
        Snapshot.State != EGamePlatformServerLifecycleState::Ready &&
        Snapshot.State != EGamePlatformServerLifecycleState::Registering &&
        Snapshot.State != EGamePlatformServerLifecycleState::PublishingReady)
    {
        return false;
    }

    // Drain优先于周期心跳；控制操作须串行，调用者应先关闭本地准入，再等控制面排空确认。
    StopHeartbeatPump();
    if (bControlOperationInFlight || PendingControlRetryOperation.IsSet())
    {
        // 不能并发Drain与Register/Ready。保留原操作过渡态以便瞬时失败仍可重试，但排空意图持续阻止Ready发布。
        DeferredControlOperation = EControlOperation::BeginDrain;
        return true;
    }
    if (bHeartbeatInFlight)
    {
        DeferredControlOperation = EControlOperation::BeginDrain;
        SetState(EGamePlatformServerLifecycleState::Draining);
        return true;
    }
    return StartProviderOperation(EControlOperation::BeginDrain);
}

bool UGamePlatformServerLifecycleSubsystem::CompleteDrain()
{
    check(IsInGameThread());
    if (bIsClosing) return false;
    if (Snapshot.State != EGamePlatformServerLifecycleState::Draining)
    {
        return false;
    }
    if (bControlOperationInFlight || bHeartbeatInFlight ||
        ControlRetryTickerHandle.IsValid() || PendingControlRetryOperation.IsSet() ||
        DeferredControlOperation.IsSet())
    {
        // 只有控制面已确认Drain且没有待重试操作时，才允许结束本地生命周期。
        return false;
    }
    StopHeartbeatPump();
    CancelControlRetry();
    ++Generation;
    ActiveInstance = FGamePlatformServerInstanceInfo();
    SetState(EGamePlatformServerLifecycleState::Stopped);
    return true;
}

bool UGamePlatformServerLifecycleSubsystem::TickHeartbeat(float)
{
    if (bIsClosing) return false;
    check(IsInGameThread());
    const bool bLifecycleAllowsHeartbeat =
        Snapshot.State == EGamePlatformServerLifecycleState::Registered ||
        Snapshot.State == EGamePlatformServerLifecycleState::Ready ||
        Snapshot.State == EGamePlatformServerLifecycleState::Draining;
    if (!bLifecycleAllowsHeartbeat || !HeartbeatPlayerCountProvider)
    {
        // 返回false会由Ticker自行注销；只清本地句柄，避免回调内二次Remove当前Ticker。
        HeartbeatTickerHandle.Reset();
        HeartbeatPlayerCountProvider = nullptr;
        return false;
    }

    // 外部人数提供函数可以触发关闭；复制Callable并复查代次，避免销毁正在执行的成员或继续发旧请求。
    const auto CountProvider = HeartbeatPlayerCountProvider;
    const uint64 CountGeneration = Generation;
    const int32 CurrentPlayers = CountProvider();
    if (bIsClosing || CountGeneration != Generation || !HeartbeatPlayerCountProvider) return !bIsClosing;
    if (CurrentPlayers < 0 || CurrentPlayers > ActiveInstance.Capacity)
    {
        HeartbeatTickerHandle.Reset();
        HeartbeatPlayerCountProvider = nullptr;
        SetState(
            EGamePlatformServerLifecycleState::Failed,
            TEXT("HeartbeatPlayerCountInvalid"));
        return false;
    }

    // 上一请求尚在飞行或Ready/Drain正在切换时跳过本周期，不排队，不制造网络积压。
    SendHeartbeat(CurrentPlayers);
    return true;
}

bool UGamePlatformServerLifecycleSubsystem::IsTransientControlError(FName ErrorCode)
{
    return ErrorCode == FName(TEXT("ControlPlaneTransportFailed")) ||
        ErrorCode == FName(TEXT("ControlPlaneRequestStartFailed")) ||
        ErrorCode == FName(TEXT("ControlPlaneRetryableHttpStatus"));
}

float UGamePlatformServerLifecycleSubsystem::ComputeControlRetryDelaySeconds() const
{
    // 0.5、1、2、4秒后封顶8秒；加入基于实例ID的稳定抖动，避免大量服务器同时恢复造成惊群。
    const int32 Exponent = FMath::Clamp(Snapshot.ConsecutiveControlFailures - 1, 0, 4);
    const float BaseDelay = FMath::Min(8.0f, 0.5f * static_cast<float>(1 << Exponent));
    const uint32 StableHash = GetTypeHash(ActiveInstance.GameServerId);
    const float JitterUnit = static_cast<float>(StableHash % 2001) / 10000.0f - 0.1f;
    return FMath::Clamp(BaseDelay * (1.0f + JitterUnit), 0.4f, 8.8f);
}

void UGamePlatformServerLifecycleSubsystem::ScheduleControlRetry(EControlOperation Operation)
{
    check(IsInGameThread());
    if (bIsClosing) return;
    PendingControlRetryOperation = Operation;
    if (ControlRetryTickerHandle.IsValid())
    {
        return;
    }
    ControlRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(
            this,
            &UGamePlatformServerLifecycleSubsystem::TickControlRetry),
        ComputeControlRetryDelaySeconds());
    if (!ControlRetryTickerHandle.IsValid())
    {
        PendingControlRetryOperation.Reset();
        SetState(
            EGamePlatformServerLifecycleState::Failed,
            TEXT("ControlRetryScheduleFailed"));
    }
}

void UGamePlatformServerLifecycleSubsystem::CancelControlRetry()
{
    check(IsInGameThread());
    if (ControlRetryTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ControlRetryTickerHandle);
        ControlRetryTickerHandle.Reset();
    }
    PendingControlRetryOperation.Reset();
}

bool UGamePlatformServerLifecycleSubsystem::TickControlRetry(float)
{
    check(IsInGameThread());
    if (bIsClosing) return false;
    ControlRetryTickerHandle.Reset();
    if (!PendingControlRetryOperation.IsSet())
    {
        return false;
    }

    const EControlOperation Operation = PendingControlRetryOperation.GetValue();
    PendingControlRetryOperation.Reset();
    const bool bStateStillMatches =
        (Operation == EControlOperation::Register &&
            Snapshot.State == EGamePlatformServerLifecycleState::Registering) ||
        (Operation == EControlOperation::Ready &&
            Snapshot.State == EGamePlatformServerLifecycleState::PublishingReady) ||
        (Operation == EControlOperation::BeginDrain &&
            Snapshot.State == EGamePlatformServerLifecycleState::Draining);
    if (!bStateStillMatches || Snapshot.State == EGamePlatformServerLifecycleState::Stopped ||
        Snapshot.State == EGamePlatformServerLifecycleState::Failed)
    {
        return false;
    }

    if (!StartProviderOperation(Operation) &&
        Snapshot.State != EGamePlatformServerLifecycleState::Failed &&
        Snapshot.State != EGamePlatformServerLifecycleState::Stopped)
    {
        // 极短竞态不丢失恢复意图；真正配置错误会由StartProviderOperation推进Failed并停止重试。
        ScheduleControlRetry(Operation);
    }
    return false;
}

void UGamePlatformServerLifecycleSubsystem::TryStartDeferredControlOperation()
{
    check(IsInGameThread());
    if (bIsClosing || !DeferredControlOperation.IsSet() || bHeartbeatInFlight || bControlOperationInFlight ||
        Snapshot.State == EGamePlatformServerLifecycleState::Failed ||
        Snapshot.State == EGamePlatformServerLifecycleState::Stopped)
    {
        return;
    }

    const EControlOperation Operation = DeferredControlOperation.GetValue();
    DeferredControlOperation.Reset();
    if (!StartProviderOperation(Operation) &&
        Snapshot.State != EGamePlatformServerLifecycleState::Failed &&
        Snapshot.State != EGamePlatformServerLifecycleState::Stopped)
    {
        ++Snapshot.ConsecutiveControlFailures;
        ScheduleControlRetry(Operation);
    }
}

IGamePlatformServerControlProvider*
UGamePlatformServerLifecycleSubsystem::ResolveUniqueProvider()
{
    TArray<IGamePlatformServerControlProvider*> Providers =
        IModularFeatures::Get().GetModularFeatureImplementations<
            IGamePlatformServerControlProvider>(
                IGamePlatformServerControlProvider::GetModularFeatureName());
    return Providers.Num() == 1 ? Providers[0] : nullptr;
}

bool UGamePlatformServerLifecycleSubsystem::StartProviderOperation(
    EControlOperation Operation,
    int32 CurrentPlayers)
{
    check(IsInGameThread());
    if (bIsClosing) return false;
    IGamePlatformServerControlProvider* Provider = ResolveUniqueProvider();
    if (!Provider)
    {
        bHeartbeatInFlight = false;
        bControlOperationInFlight = false;
        SetState(
            EGamePlatformServerLifecycleState::Failed,
            TEXT("ControlProviderUnavailableOrAmbiguous"));
        return false;
    }

    if (Operation == EControlOperation::Heartbeat)
    {
        if (bControlOperationInFlight)
        {
            bHeartbeatInFlight = false;
            return false;
        }
    }
    else
    {
        if (bControlOperationInFlight || bHeartbeatInFlight)
        {
            return false;
        }
        bControlOperationInFlight = true;
    }

    ++Generation;
    const uint64 OperationGeneration = Generation;
    switch (Operation)
    {
    case EControlOperation::Register:
        SetState(EGamePlatformServerLifecycleState::Registering);
        break;
    case EControlOperation::Ready:
        SetState(EGamePlatformServerLifecycleState::PublishingReady);
        break;
    case EControlOperation::BeginDrain:
        SetState(EGamePlatformServerLifecycleState::Draining);
        break;
    case EControlOperation::Heartbeat:
    default:
        break;
    }

    // 状态通知允许关闭/重新推进实例；通知后再次核对所有权，关闭期间不能继续创建原操作的HTTP。
    if (bIsClosing || OperationGeneration != Generation) return false;
    const TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> WeakThis(this);
    FGamePlatformServerControlCompletion Completion =
        [WeakThis, OperationGeneration, Operation](bool bSucceeded, FName ErrorCode)
        {
            AsyncTask(ENamedThreads::GameThread,
                [WeakThis, OperationGeneration, Operation, bSucceeded, ErrorCode]()
                {
                    if (WeakThis.IsValid())
                    {
                        WeakThis->CompleteOperation(
                            OperationGeneration,
                            Operation,
                            bSucceeded,
                            ErrorCode);
                    }
                });
        };

    switch (Operation)
    {
    case EControlOperation::Register:
        Provider->RegisterInstance(ActiveInstance, MoveTemp(Completion));
        break;
    case EControlOperation::Heartbeat:
        {
            const FString Status = Snapshot.State == EGamePlatformServerLifecycleState::Ready
                ? TEXT("Ready")
                : Snapshot.State == EGamePlatformServerLifecycleState::Draining
                    ? TEXT("Draining")
                    : TEXT("Starting");
            Provider->SendHeartbeat(
                ActiveInstance,
                CurrentPlayers,
                Status,
                MoveTemp(Completion));
        }
        break;
    case EControlOperation::Ready:
        Provider->PublishReady(ActiveInstance, MoveTemp(Completion));
        break;
    case EControlOperation::BeginDrain:
        Provider->BeginDrain(ActiveInstance, MoveTemp(Completion));
        break;
    }
    return true;
}

void UGamePlatformServerLifecycleSubsystem::CompleteOperation(
    uint64 OperationGeneration,
    EControlOperation Operation,
    bool bSucceeded,
    FName ErrorCode)
{
    check(IsInGameThread());
    if (bIsClosing || OperationGeneration != Generation)
    {
        return;
    }
    if (Operation == EControlOperation::Heartbeat)
    {
        bHeartbeatInFlight = false;
        if (!bSucceeded)
        {
            const FName EffectiveError =
                ErrorCode.IsNone() ? FName(TEXT("HeartbeatFailed")) : ErrorCode;
            ++Snapshot.ConsecutiveHeartbeatFailures;
            if (IsTransientControlError(EffectiveError))
            {
                // 短暂断网、超时、429/5xx不把已运行世界永久打死；下一Heartbeat周期自然重试。
                SetState(Snapshot.State, EffectiveError);
                TryStartDeferredControlOperation();
                return;
            }
            DeferredControlOperation.Reset();
            SetState(EGamePlatformServerLifecycleState::Failed, EffectiveError);
            return;
        }

        if (Snapshot.ConsecutiveHeartbeatFailures > 0 || !Snapshot.ErrorCode.IsNone())
        {
            Snapshot.ConsecutiveHeartbeatFailures = 0;
            SetState(Snapshot.State, NAME_None);
        }
        TryStartDeferredControlOperation();
        return;
    }
    bControlOperationInFlight = false;
    if (!bSucceeded)
    {
        const FName EffectiveError =
            ErrorCode.IsNone() ? FName(TEXT("ControlOperationFailed")) : ErrorCode;
        ++Snapshot.ConsecutiveControlFailures;
        if (IsTransientControlError(EffectiveError))
        {
            // 注册、Ready和Drain均保持当前过渡态，以指数退避重试；不会逐帧重试或创建并发请求。
            ScheduleControlRetry(Operation);
            // 先发布重试所有权；通知中BeginDrain只排队，Deinitialize能完整取消已建立的Ticker。
            if (bIsClosing || OperationGeneration != Generation ||
                Snapshot.State == EGamePlatformServerLifecycleState::Failed) return;
            SetState(Snapshot.State, EffectiveError);
            return;
        }
        CancelControlRetry();
        DeferredControlOperation.Reset();
        SetState(EGamePlatformServerLifecycleState::Failed, EffectiveError);
        return;
    }

    CancelControlRetry();
    Snapshot.ConsecutiveControlFailures = 0;

    if (Operation != EControlOperation::BeginDrain &&
        DeferredControlOperation.IsSet() &&
        DeferredControlOperation.GetValue() == EControlOperation::BeginDrain)
    {
        // 注册/Ready确认到达时优先执行已请求的Drain，禁止先广播Ready造成心跳或新准入短暂复活。
        SetState(EGamePlatformServerLifecycleState::Draining);
        TryStartDeferredControlOperation();
        return;
    }

    switch (Operation)
    {
    case EControlOperation::Register:
        SetState(EGamePlatformServerLifecycleState::Registered);
        break;
    case EControlOperation::Ready:
        SetState(EGamePlatformServerLifecycleState::Ready);
        break;
    case EControlOperation::BeginDrain:
        SetState(EGamePlatformServerLifecycleState::Draining);
        break;
    case EControlOperation::Heartbeat:
        break;
    }
}

void UGamePlatformServerLifecycleSubsystem::SetState(
    EGamePlatformServerLifecycleState State,
    FName ErrorCode)
{
    Snapshot.State = State;
    Snapshot.ErrorCode = ErrorCode;
    Snapshot.OperationGeneration = Generation;
    const FGamePlatformServerLifecycleSnapshot Published = Snapshot;
    LifecycleChanged.Broadcast(Published);
}
