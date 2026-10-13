// Agones HTTP SDK 适配实现：所有请求只发往 127.0.0.1 的受控端口。
// MainArena 原子分配归 Go 的 CRD 处理；共享世界首次有玩家时调用本地 SDK Allocate，防止 Counter 缩容误删活跃实例。
#include "Agones/GamePlatformAgonesBridgeSubsystem.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DefaultValueHelper.h"

bool UGamePlatformAgonesBridgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Agones Sidecar 会自动注入此变量；普通 Dedicated、Editor 和 Commandlet 不创建适配器。
    const FString Port = FPlatformMisc::GetEnvironmentVariable(TEXT("AGONES_SDK_HTTP_PORT"));
    return IsRunningDedicatedServer() && !IsRunningCommandlet() && !Port.IsEmpty() &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformAgonesBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FString PortText = FPlatformMisc::GetEnvironmentVariable(TEXT("AGONES_SDK_HTTP_PORT"));
    if (!FDefaultValueHelper::ParseInt(PortText, SdkHttpPort) ||
        SdkHttpPort < 1 || SdkHttpPort > 65535)
    {
        // 配置失效时不得发送 Ready；侧车健康探测会将失联服务器移出可用池。
        UE_LOG(LogTemp, Error, TEXT("Agones SDK HTTP port invalid; GameServer will not be marked Ready"));
        return;
    }

    CounterKey = FPlatformMisc::GetEnvironmentVariable(TEXT("AGONES_PLAYER_COUNTER_KEY"));
    if (!CounterKey.IsEmpty())
    {
        // 路径仅允许小写 ASCII 标识，避免配置内容变成任意 SDK 路径或查询参数。
        if (CounterKey.Len() > 63 || !FChar::IsLower(CounterKey[0]))
        {
            UE_LOG(LogTemp, Error, TEXT("Agones player counter key invalid"));
            return;
        }
        for (const TCHAR Character : CounterKey)
        {
            if (!((Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('-')))
            {
                UE_LOG(LogTemp, Error, TEXT("Agones player counter key invalid"));
                return;
            }
        }
    }

    const FString Interval = FPlatformMisc::GetEnvironmentVariable(TEXT("AGONES_HEALTH_INTERVAL_SECONDS"));
    if (!Interval.IsEmpty())
    {
        int32 ParsedInterval = 0;
        if (!FDefaultValueHelper::ParseInt(Interval, ParsedInterval) ||
            ParsedInterval < 1 || ParsedInterval > 8)
        {
            UE_LOG(LogTemp, Error, TEXT("Agones health interval must be 1-8 seconds"));
            return;
        }
        HealthIntervalSeconds = static_cast<double>(ParsedInterval);
    }

    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance() ? GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>() : nullptr;
    if (!Lifecycle)
    {
        UE_LOG(LogTemp, Error, TEXT("Agones requires GamePlatformServerLifecycleSubsystem"));
        return;
    }

    LifecycleChangedHandle = Lifecycle->OnLifecycleChanged().AddUObject(
        this, &ThisClass::HandleServerLifecycle);
    PlayerCountHandle = Lifecycle->OnPlayerCountSampled().AddUObject(
        this, &ThisClass::HandlePlayerCountSampled);
    bEnabled = true;
    NextHealthAtSeconds = FPlatformTime::Seconds();
    NextRetryAtSeconds = NextHealthAtSeconds;
    HandleServerLifecycle(Lifecycle->GetSnapshot());

    // 每秒调度一次，不是逐帧 Tick；HTTP 请求单飞，失败有限退避，不构造无界任务队列。
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::TickSdk), 1.0f);
    if (!TickerHandle.IsValid())
    {
        bEnabled = false;
        UE_LOG(LogTemp, Error, TEXT("Agones scheduler unavailable; GameServer will not be marked Ready"));
    }
}

void UGamePlatformAgonesBridgeSubsystem::Deinitialize()
{
    bClosing = true;
    bEnabled = false;
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    if (UGameInstance* Instance = GetGameInstance())
    {
        if (UGamePlatformServerLifecycleSubsystem* Lifecycle =
                Instance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>())
        {
            if (LifecycleChangedHandle.IsValid())
            {
                Lifecycle->OnLifecycleChanged().Remove(LifecycleChangedHandle);
            }
            if (PlayerCountHandle.IsValid())
            {
                Lifecycle->OnPlayerCountSampled().Remove(PlayerCountHandle);
            }
        }
    }
    LifecycleChangedHandle.Reset();
    PlayerCountHandle.Reset();
    CancelSdkRequest();
    Super::Deinitialize();
}

void UGamePlatformAgonesBridgeSubsystem::HandleServerLifecycle(
    const FGamePlatformServerLifecycleSnapshot& Snapshot)
{
    if (!bEnabled || bClosing)
    {
        return;
    }

    if (Snapshot.State == EGamePlatformServerLifecycleState::Ready)
    {
        // 已进入排空/关闭的实例不可被迟到 Ready 重新激活。
        if (bDraining || bShutdownRequested) return;
        // Go 控制面 Ready 和必要资源门禁先完成，之后才向 Agones 发布可分配。
        bReadyRequested = true;
        return;
    }

    if (Snapshot.State == EGamePlatformServerLifecycleState::Draining ||
        Snapshot.State == EGamePlatformServerLifecycleState::Stopped ||
        Snapshot.State == EGamePlatformServerLifecycleState::Failed)
    {
        bDraining = true;
        bReadyRequested = false;
        bNeedsSelfAllocation = false;
        // 进入排空立即撤销待发 Ready；Agones 分配后仍须受 Go 的准入核验约束。
        if (ActiveRequest.IsValid() && ActiveOperation == EAgonesSdkOperation::Ready)
        {
            CancelSdkRequest();
        }
        if (!CounterKey.IsEmpty() && Snapshot.State == EGamePlatformServerLifecycleState::Draining &&
            LatestCapacity > 0)
        {
            // 排空服务器不得继续对扩容算法贡献空闲名额。
            LatestPlayers = LatestCapacity;
            bCounterDirty = true;
        }
        if (Snapshot.State == EGamePlatformServerLifecycleState::Stopped)
        {
            // 只有平台 CompleteDrain 成功才通知 Agones 终止 Pod；比赛结束不能直接杀进程。
            bShutdownRequested = true;
            NextRetryAtSeconds = FPlatformTime::Seconds();
        }
    }
}

void UGamePlatformAgonesBridgeSubsystem::HandlePlayerCountSampled(int32 CurrentPlayers, int32 Capacity)
{
    if (!bEnabled || bClosing || CounterKey.IsEmpty() || bDraining || bShutdownRequested ||
        CurrentPlayers < 0 || Capacity < 1 || CurrentPlayers > Capacity)
    {
        return;
    }
    LatestPlayers = CurrentPlayers;
    LatestCapacity = Capacity;
    bCounterDirty = true;
    // RegistryAllocator 可把多个玩家分配进同一个常驻世界；有玩家后把 Agones 标为 Allocated，
    // 避免 Fleet 缩容移走活跃世界。零玩家并不能证明所有迁移票据已撤销，故不自动改回 Ready。
    if (CurrentPlayers > 0 && !bSelfAllocated)
    {
        bNeedsSelfAllocation = true;
    }
}

bool UGamePlatformAgonesBridgeSubsystem::TickSdk(float)
{
    if (bClosing || !bEnabled)
    {
        return false;
    }
    if (ActiveRequest.IsValid())
    {
        return true;
    }

    const double Now = FPlatformTime::Seconds();
    if (bShutdownRequested)
    {
        if (!bShutdownAcknowledged && Now >= NextRetryAtSeconds)
        {
            StartSdkRequest(EAgonesSdkOperation::Shutdown);
        }
        return true;
    }

    // Sidecar 健康与业务 Ready 是独立含义；加载世界期间也要真实报告进程存活。
    if (Now >= NextHealthAtSeconds)
    {
        NextHealthAtSeconds = Now + HealthIntervalSeconds;
        StartSdkRequest(EAgonesSdkOperation::Health);
        return true;
    }
    if (bReadyRequested && !bReadyAcknowledged && Now >= NextRetryAtSeconds)
    {
        StartSdkRequest(EAgonesSdkOperation::Ready);
        return true;
    }
    if (bReadyAcknowledged && bNeedsSelfAllocation && !bSelfAllocated &&
        !bDraining && Now >= NextRetryAtSeconds)
    {
        StartSdkRequest(EAgonesSdkOperation::Allocate);
        return true;
    }
    if (bReadyAcknowledged && bCounterDirty && !CounterKey.IsEmpty() &&
        LatestPlayers >= 0 && LatestCapacity > 0 && Now >= NextRetryAtSeconds)
    {
        StartSdkRequest(EAgonesSdkOperation::Counter);
    }
    return true;
}

void UGamePlatformAgonesBridgeSubsystem::StartSdkRequest(EAgonesSdkOperation Operation)
{
    check(IsInGameThread());
    if (bClosing || !bEnabled || ActiveRequest.IsValid() ||
        SdkHttpPort < 1 || SdkHttpPort > 65535)
    {
        return;
    }

    FString Path;
    FString Verb = TEXT("POST");
    FString Body = TEXT("{}");
    switch (Operation)
    {
    case EAgonesSdkOperation::Health:
        Path = TEXT("/health");
        break;
    case EAgonesSdkOperation::Ready:
        Path = TEXT("/ready");
        break;
    case EAgonesSdkOperation::Shutdown:
        Path = TEXT("/shutdown");
        break;
    case EAgonesSdkOperation::Allocate:
        Path = TEXT("/allocate");
        break;
    case EAgonesSdkOperation::Counter:
        Path = TEXT("/v1beta1/counters/") + CounterKey;
        Verb = TEXT("PATCH");
        Body = FString::Printf(TEXT("{\"count\":\"%d\",\"capacity\":\"%d\"}"),
            LatestPlayers, LatestCapacity);
        break;
    }

    const int32 SamplePlayers = LatestPlayers;
    const int32 SampleCapacity = LatestCapacity;
    const uint64 ThisRequest = ++RequestGeneration;
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        NextRetryAtSeconds = FPlatformTime::Seconds() + 5.0;
        return;
    }
#else
    // 不允许侧车请求意外跟随重定向，防止访问任意本地服务。
    NextRetryAtSeconds = FPlatformTime::Seconds() + 5.0;
    return;
#endif

    Request->SetVerb(Verb);
    Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d%s"), SdkHttpPort, *Path));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetTimeout(2.0f);
    Request->SetContentAsString(Body);
    ActiveOperation = Operation;
    ActiveRequest = Request;

    const TWeakObjectPtr<UGamePlatformAgonesBridgeSubsystem> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, ThisRequest, Operation, SamplePlayers, SampleCapacity](
            FHttpRequestPtr, FHttpResponsePtr Response, bool bTransportSucceeded)
        {
            const bool bSucceeded = bTransportSucceeded && Response.IsValid() &&
                Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300;
            // HTTP 默认回调通常在游戏线程；保险起见统一回到游戏线程再修改 GameInstance 状态。
            const auto CompleteOnGameThread =
                [WeakThis, ThisRequest, Operation, bSucceeded, SamplePlayers, SampleCapacity]()
                {
                    if (UGamePlatformAgonesBridgeSubsystem* Self = WeakThis.Get())
                    {
                        Self->CompleteSdkRequest(ThisRequest, Operation,
                            bSucceeded, SamplePlayers, SampleCapacity);
                    }
                };
            if (IsInGameThread())
            {
                CompleteOnGameThread();
            }
            else
            {
                AsyncTask(ENamedThreads::GameThread, CompleteOnGameThread);
            }
        });

    if (!Request->ProcessRequest())
    {
        Request->OnProcessRequestComplete().Unbind();
        CompleteSdkRequest(ThisRequest, Operation, false, SamplePlayers, SampleCapacity);
    }
}

void UGamePlatformAgonesBridgeSubsystem::CompleteSdkRequest(
    uint64 RequestId, EAgonesSdkOperation Operation,
    bool bSucceeded, int32 SamplePlayers, int32 SampleCapacity)
{
    check(IsInGameThread());
    if (bClosing || RequestId != RequestGeneration || !ActiveRequest.IsValid())
    {
        return;
    }
    ActiveRequest->OnProcessRequestComplete().Unbind();
    ActiveRequest.Reset();
    if (!bSucceeded)
    {
        // Sidecar 可能晚于游戏进程启动；失败仅对同一意图重试，永不伪造 SDK 成功。
        NextRetryAtSeconds = FPlatformTime::Seconds() + 5.0;
        return;
    }

    switch (Operation)
    {
    case EAgonesSdkOperation::Ready:
        if (bReadyRequested && !bShutdownRequested)
        {
            bReadyAcknowledged = true;
        }
        break;
    case EAgonesSdkOperation::Allocate:
        if (!bDraining && !bShutdownRequested)
        {
            bSelfAllocated = true;
            bNeedsSelfAllocation = false;
        }
        break;
    case EAgonesSdkOperation::Counter:
        if (SamplePlayers == LatestPlayers && SampleCapacity == LatestCapacity)
        {
            bCounterDirty = false;
        }
        break;
    case EAgonesSdkOperation::Shutdown:
        bShutdownAcknowledged = true;
        break;
    case EAgonesSdkOperation::Health:
        break;
    }
}

void UGamePlatformAgonesBridgeSubsystem::CancelSdkRequest()
{
    check(IsInGameThread());
    ++RequestGeneration;
    if (ActiveRequest.IsValid())
    {
        const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = MoveTemp(ActiveRequest);
        Request->OnProcessRequestComplete().Unbind();
        Request->CancelRequest();
    }
}
