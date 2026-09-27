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
        IsBoundedValue(ServerRoleId) && IsBoundedValue(ExperienceId) &&
        IsBoundedValue(WorldId) && IsBoundedValue(RegionId) &&
        IsOptionalBoundedValue(ClusterId) && IsOptionalBoundedValue(NodeId) &&
        IsBoundedValue(BuildVersion) && IsBoundedValue(PublicEndpoint) &&
        ProtocolVersion >= 0 && Capacity > 0;
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
    if (Snapshot.State != EGamePlatformServerLifecycleState::Unregistered ||
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
    if (!bCanSend || bHeartbeatInFlight || CurrentPlayers < 0 ||
        CurrentPlayers > ActiveInstance.Capacity)
    {
        return false;
    }
    bHeartbeatInFlight = true;
    return StartProviderOperation(EControlOperation::Heartbeat, CurrentPlayers);
}

bool UGamePlatformServerLifecycleSubsystem::MarkReady()
{
    check(IsInGameThread());
    if (Snapshot.State != EGamePlatformServerLifecycleState::Registered)
    {
        return false;
    }
    return StartProviderOperation(EControlOperation::Ready);
}

bool UGamePlatformServerLifecycleSubsystem::BeginDrain()
{
    check(IsInGameThread());
    if (Snapshot.State != EGamePlatformServerLifecycleState::Registered &&
        Snapshot.State != EGamePlatformServerLifecycleState::Ready)
    {
        return false;
    }
    return StartProviderOperation(EControlOperation::BeginDrain);
}

bool UGamePlatformServerLifecycleSubsystem::CompleteDrain()
{
    check(IsInGameThread());
    if (Snapshot.State != EGamePlatformServerLifecycleState::Draining)
    {
        return false;
    }
    if (bControlOperationInFlight || bHeartbeatInFlight)
    {
        return false;
    }
    ++Generation;
    ActiveInstance = FGamePlatformServerInstanceInfo();
    SetState(EGamePlatformServerLifecycleState::Stopped);
    return true;
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
    if (OperationGeneration != Generation)
    {
        return;
    }
    if (Operation == EControlOperation::Heartbeat)
    {
        bHeartbeatInFlight = false;
        if (!bSucceeded)
        {
            SetState(EGamePlatformServerLifecycleState::Failed,
                ErrorCode.IsNone() ? FName(TEXT("HeartbeatFailed")) : ErrorCode);
        }
        return;
    }
    bControlOperationInFlight = false;
    if (!bSucceeded)
    {
        SetState(EGamePlatformServerLifecycleState::Failed,
            ErrorCode.IsNone() ? FName(TEXT("ControlOperationFailed")) : ErrorCode);
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
    LifecycleChanged.Broadcast(Snapshot);
}
