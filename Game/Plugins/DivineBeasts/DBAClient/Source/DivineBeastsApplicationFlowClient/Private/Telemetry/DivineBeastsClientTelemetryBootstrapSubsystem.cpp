#include "Telemetry/DivineBeastsClientTelemetryBootstrapSubsystem.h"

#include "GamePlatformOnlineClientSubsystem.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Flow/DivineBeastsFlowNodes.h"
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

    GatewayBaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("DIVINEBEASTS_GATEWAY_BASE_URL"));
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));

    const FString ContentRevision = FPlatformMisc::GetEnvironmentVariable(TEXT("DIVINEBEASTS_CONTENT_REVISION"));
    if (!ContentRevision.IsEmpty())
    {
        Telemetry->SetContentRevision(ContentRevision);
    }

    // 处理组合子系统创建前已经完成认证/世界分配的情况；先准备非秘密URL，再同步认证，确保已登录用户直接绑定当前认证代次的Sink。
    HandleAuthStateChanged(Online->GetSnapshot());
    if (ApplicationFlow)
    {
        HandleFlowViewStateChanged(ApplicationFlow->GetViewState());
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
        const FGuid NewGeneration = Snapshot.AuthGeneration;
        const bool bAuthGenerationChanged =
            TelemetryAuthGeneration.IsValid() && NewGeneration.IsValid() &&
            TelemetryAuthGeneration != NewGeneration;

        if (bAuthGenerationChanged && !TelemetrySessionId.IsEmpty())
        {
            // 账号切换时先终止旧会话并立即Shutdown旧Sink；否则旧Batch可能在断线恢复后使用新账号Token发送，造成归属串号。
            Telemetry->EndSession();
            SwitchToNullSink();
            Telemetry->DiscardBufferedRecordsForPrivacyBoundary();
            TelemetrySessionId.Reset();
            TelemetryAuthGeneration.Invalidate();
        }

        if (TelemetrySessionId.IsEmpty())
        {
            bConfiguredNetworkSink = ConfigureNetworkSinkForAuthenticatedSession();
            TelemetryAuthGeneration = NewGeneration;
            TelemetrySessionId = NewGeneration.IsValid()
                ? NewGeneration.ToString(EGuidFormats::Digits)
                : FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Telemetry->BeginSession(TelemetrySessionId, FString());

            FGamePlatformTelemetryEvent Started;
            Started.EventName = TEXT("Telemetry.Foundation.ClientStarted");
            Telemetry->RecordEvent(MoveTemp(Started));
        }
        return;
    }

    if (!TelemetrySessionId.IsEmpty() &&
        (Snapshot.State == EGamePlatformAuthState::LoggedOut ||
         Snapshot.State == EGamePlatformAuthState::Failed))
    {
        Telemetry->EndSession();
        SwitchToNullSink();
        Telemetry->DiscardBufferedRecordsForPrivacyBoundary();
        TelemetrySessionId.Reset();
        TelemetryAuthGeneration.Invalidate();
    }
}

bool UDivineBeastsClientTelemetryBootstrapSubsystem::ConfigureNetworkSinkForAuthenticatedSession()
{
    check(IsInGameThread());
    UGameInstance* GameInstance = GetGameInstance();
    UGamePlatformTelemetrySubsystem* Telemetry =
        GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr;
    UGamePlatformOnlineClientSubsystem* Online = OnlineSubsystem.Get();
    if (!Telemetry || !Online || GatewayBaseUrl.IsEmpty())
    {
        // 本地无后端或认证子系统不可用时保留NullSink；遥测失败不能影响登录/玩法。
        return false;
    }

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakOnline = Online;
    FGamePlatformTelemetryRequestAuthorizer RequestAuthorizer =
        [WeakOnline](IHttpRequest& Request)
        {
            const UGamePlatformOnlineClientSubsystem* CurrentOnline = WeakOnline.Get();
            return CurrentOnline && CurrentOnline->ApplyAuthorization(Request);
        };

    const TSharedRef<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe>(
            GatewayBaseUrl,
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
    return Telemetry->ConfigureSink(
        MakeShared<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe>(Transport, Retry));
}

void UDivineBeastsClientTelemetryBootstrapSubsystem::SwitchToNullSink()
{
    check(IsInGameThread());
    UGameInstance* GameInstance = GetGameInstance();
    if (UGamePlatformTelemetrySubsystem* Telemetry =
            GameInstance ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>() : nullptr)
    {
        Telemetry->ConfigureSink(
            MakeShared<FGamePlatformTelemetryNullSink, ESPMode::ThreadSafe>());
    }
    bConfiguredNetworkSink = false;
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

    const FName CurrentStep = ViewState.CurrentStep;
    if (CurrentStep == FDivineBeastsFlowNodes::TransferWorld() &&
        LastObservedFlowStep != CurrentStep)
    {
        // 进入真实Travel边界时先尽力刷新旧世界记录并清空旧World Context，避免切服期间事件被错误归属。
        Telemetry->BeforeWorldTravel();
    }

    if (CurrentStep == FDivineBeastsFlowNodes::InWorld())
    {
        const FDivineBeastsWorldAssignmentSummary& Assignment = ViewState.Assignment;
        if (!Assignment.WorldId.IsNone() && !Assignment.ExperienceId.IsNone() && !Assignment.MapId.IsNone())
        {
            // 只有WorldReady完成、正式进入InWorld后才提交新世界上下文；公开摘要不含Endpoint/Ticket。
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
    }

    LastObservedFlowStep = CurrentStep;
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
    if (bConfiguredNetworkSink)
    {
        // Deinitialize同样切断旧认证代次的Retry链；Telemetry子系统随后仍会执行自身有界关停。
        SwitchToNullSink();
    }
    TelemetryAuthGeneration.Invalidate();
    GatewayBaseUrl.Reset();
    LastObservedFlowStep = NAME_None;
    bConfiguredNetworkSink = false;
    Super::Deinitialize();
}
