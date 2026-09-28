#include "Telemetry/DivineBeastsClientTelemetryBootstrapSubsystem.h"

#include "GamePlatformOnlineClientSubsystem.h"
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
    if (!Telemetry || !Online)
    {
        return;
    }

    FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("DIVINEBEASTS_GATEWAY_BASE_URL"));
    BaseUrl.RemoveFromEnd(TEXT("/"));
    if (BaseUrl.IsEmpty())
    {
        // 本地无后端时保持NullSink；遥测缺失绝不能阻断客户端启动。
        return;
    }

    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> WeakOnline = Online;
    FGamePlatformTelemetryHeaderProvider HeaderProvider = [WeakOnline]()
    {
        TMap<FString, FString> Headers;
        if (const UGamePlatformOnlineClientSubsystem* CurrentOnline = WeakOnline.Get())
        {
            const FString Authorization = CurrentOnline->GetAuthorizationHeaderValueTransient();
            if (!Authorization.IsEmpty())
            {
                Headers.Add(TEXT("Authorization"), Authorization);
            }
        }
        return Headers;
    };

    const TSharedRef<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe>(
            BaseUrl,
            TEXT("/telemetry/v1/batches"),
            TMap<FString, FString>{},
            5.0f,
            256 * 1024,
            MoveTemp(HeaderProvider));

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

void UDivineBeastsClientTelemetryBootstrapSubsystem::Deinitialize()
{
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
    bConfiguredNetworkSink = false;
    Super::Deinitialize();
}
