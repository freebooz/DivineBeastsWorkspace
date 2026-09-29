#include "Telemetry/DivineBeastsClientTelemetryBootstrapSubsystem.h"

#include "GamePlatformOnlineClientSubsystem.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "HAL/PlatformMisc.h"
#include "Sinks/GamePlatformTelemetryNetworkSink.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Transport/GamePlatformTelemetryTransport.h"

bool UDivineBeastsClientTelemetryBootstrapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return !IsRunningDedicatedServer() && !IsRunningCommandlet() && Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsClientTelemetryBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UGameInstance* GameInstance = GetGameInstance();
    UGamePlatformTelemetrySubsystem* Telemetry =
        GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr;
    UGamePlatformOnlineClientSubsystem* Online =
        GameInstance ? GameInstance->GetSubsystem<UGamePlatformOnlineClientSubsystem>() : nullptr;
    UDivineBeastsApplicationFlowSubsystem* ApplicationFlow =
        GameInstance ? GameInstance->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>() : nullptr;
    if (!Telemetry || !Online)
    {
        return;
    }

    OnlineSubsystem = Online;
    ApplicationFlowSubsystem = ApplicationFlow;
    AuthStateChangedHandle = Online->OnAuthStateChanged().AddUObject(
        this,
        &UDivineBeastsClientTelemetryBootstrapSubsystem::HandleAuthStateChanged);
    if (ApplicationFlow)
    {
        FlowViewStateChangedHandle = ApplicationFlow->OnViewStateChanged().AddUObject(
            this,
            &UDivineBeastsClientTelemetryBootstrapSubsystem::HandleFlowViewStateChanged);
    }

    // 处理组合子系统创建前已经完成认证/世界分配的情况；事件驱动之外只做这一次初始快照同步。
    HandleAuthStateChanged(Online->GetSnapshot());
    if (ApplicationFlow)
    {
        HandleFlowViewStateChanged(ApplicationFlow->GetViewState());
    }

    FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("DIVINEBEASTS_GATEWAY_BASE_URL"));
    BaseUrl.RemoveFromEnd(TEXT("/"));
    if (BaseUrl.IsEmpty())
    {
        // 本地无后端时保持NullSink；遥测缺失绝不能阻断客户端启动。
        return;
    }

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakOnline = Online;
    FGamePlatformTelemetryRequestAuthorizer RequestAuthorizer =
        [WeakOnline](IHttpRequest& Request)
        {
            const UGamePlatformOnlineClientSubsystem* CurrentOnline =
                WeakOnline.Get();
            return CurrentOnline &&
                CurrentOnline->ApplyAuthorization(Request);
        };

    const TSharedRef<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe>(
            BaseUrl,
            TEXT("/telemetry/v1/batches"),
            TMap<FString, FString>{},
            5.0f,
            256 * 1024,
            FGamePlatformTelemetryHeaderProvider(),
            MoveTemp(RequestAuthorizer));

    FGamePlatformTelemetryRetrySettings Retry;
    Retry.MaxRetries = 4;
    Retry.MaxPendingBatches = 8;
    Retry.MaxRetryAgeSeconds = 30.0f;
    bConfiguredNetworkSink = Telemetry->ConfigureSink(
        MakeShared<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe>(Transport, Retry));

    const FString ContentRevision = FPlatformMisc::GetEnvironmentVariable(TEXT("DIVINEBEASTS_CONTENT_REVISION"));
    if (!ContentRevision.IsEmpty())
    {
        Telemetry->SetContentRevision(ContentRevision);
    }
}

void UDivineBeastsClientTelemetryBootstrapSubsystem::HandleAuthStateChanged(
    const FGamePlatformAuthSnapshot& Snapshot)
{
    UGameInstance* GameInstance = GetGameInstance();
    UGamePlatformTelemetrySubsystem* Telemetry =
        GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr;
    if (!Telemetry)
    {
        return;
    }

    if (Snapshot.State == EGamePlatformAuthState::Authenticated)
    {
        // 不把AccountId写入遥测。后端Gateway依据真实认证上下文生成伪匿名玩家ID并覆盖客户端身份。
        if (TelemetrySessionId.IsEmpty())
        {
            TelemetrySessionId = Snapshot.AuthGeneration.IsValid()
                ? Snapshot.AuthGeneration.ToString(EGuidFormats::Digits)
                : FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Telemetry->BeginSession(TelemetrySessionId, FString());

            FGamePlatformTelemetryEvent Started;
            Started.EventName = TEXT("Telemetry.Foundation.ClientStarted");
            Telemetry->RecordEvent(MoveTemp(Started));
        }
        return;
    }

    if (!TelemetrySessionId.IsEmpty() &&
        Snapshot.State == EGamePlatformAuthState::LoggedOut)
    {
        Telemetry->EndSession();
        TelemetrySessionId.Reset();
    }
}

void UDivineBeastsClientTelemetryBootstrapSubsystem::HandleFlowViewStateChanged(
    const FDivineBeastsFlowViewState& ViewState)
{
    UGameInstance* GameInstance = GetGameInstance();
    UGamePlatformTelemetrySubsystem* Telemetry =
        GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr;
    if (!Telemetry)
    {
        return;
    }

    const FDivineBeastsWorldAssignmentSummary& Assignment = ViewState.Assignment;
    if (Assignment.WorldId.IsNone() || Assignment.ExperienceId.IsNone() || Assignment.MapId.IsNone())
    {
        return;
    }

    // ApplicationFlow公开摘要同时提供Map/World/Experience/Region且不含Endpoint/Ticket，是项目世界遥测的正确组合边界。
    Telemetry->SetServerContext(
        Assignment.ServerRoleId.ToString(),
        Assignment.RegionId.ToString(),
        Assignment.GameServerId);
    Telemetry->UpdateWorldContext(
        Assignment.MapId.ToString(),
        Assignment.WorldId.ToString(),
        Assignment.ExperienceId.ToString(),
        FString(),
        FString());
}

void UDivineBeastsClientTelemetryBootstrapSubsystem::Deinitialize()
{
    if (UGamePlatformOnlineClientSubsystem* Online = OnlineSubsystem.Get();
        Online && AuthStateChangedHandle.IsValid())
    {
        Online->OnAuthStateChanged().Remove(AuthStateChangedHandle);
    }
    if (UDivineBeastsApplicationFlowSubsystem* ApplicationFlow = ApplicationFlowSubsystem.Get();
        ApplicationFlow && FlowViewStateChangedHandle.IsValid())
    {
        ApplicationFlow->OnViewStateChanged().Remove(FlowViewStateChangedHandle);
    }
    AuthStateChangedHandle.Reset();
    FlowViewStateChangedHandle.Reset();
    OnlineSubsystem.Reset();
    ApplicationFlowSubsystem.Reset();

    if (bConfiguredNetworkSink)
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UGamePlatformTelemetrySubsystem* Telemetry =
                    GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>())
            {
                Telemetry->FlushBestEffort();
            }
        }
    }
    if (!TelemetrySessionId.IsEmpty())
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UGamePlatformTelemetrySubsystem* Telemetry =
                    GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>())
            {
                Telemetry->EndSession();
            }
        }
        TelemetrySessionId.Reset();
    }
    bConfiguredNetworkSink = false;
    Super::Deinitialize();
}
