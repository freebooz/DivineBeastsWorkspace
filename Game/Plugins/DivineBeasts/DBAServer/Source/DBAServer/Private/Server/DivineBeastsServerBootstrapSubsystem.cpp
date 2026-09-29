#include "Server/DivineBeastsServerBootstrapSubsystem.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Sinks/GamePlatformTelemetryNetworkSink.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Transport/GamePlatformTelemetryTransport.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

bool UDivineBeastsServerBootstrapSubsystem::ShouldCreateSubsystem(
    UObject* Outer) const
{
    return IsRunningDedicatedServer() && !IsRunningCommandlet() &&
        Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsServerBootstrapSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    State = EDivineBeastsServerBootstrapState::Unconfigured;

    LoadLaunchProfile();
    if (!bHasProfile)
    {
        return;
    }

    // 遥测装配是Best Effort：缺URL/Token时保持NullSink，不能影响服务器注册与Ready门禁。
    if (UGamePlatformTelemetrySubsystem* Telemetry =
            GetGameInstance()->GetSubsystem<UGamePlatformTelemetrySubsystem>())
    {
        FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_BASE_URL"));
        BaseUrl.RemoveFromEnd(TEXT("/"));
        const FString ServerId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_ID"));
        const FString ServerRole = ActiveProfile.ServerRoleId.ToString();
        const FString Region = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_REGION_ID"));
        Telemetry->SetServerContext(ServerRole, Region, ServerId);

        if (!BaseUrl.IsEmpty())
        {
            TMap<FString, FString> StaticHeaders;
            if (!ServerId.IsEmpty())
            {
                StaticHeaders.Add(TEXT("X-Game-Server-Id"), ServerId);
            }
            if (!ServerRole.IsEmpty())
            {
                StaticHeaders.Add(TEXT("X-Server-Role"), ServerRole);
            }

            FGamePlatformTelemetryHeaderProvider HeaderProvider = []()
            {
                TMap<FString, FString> Headers;
                const FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("TELEMETRY_SERVER_INTERNAL_TOKEN"));
                if (!Token.IsEmpty() && !Token.Contains(TEXT("\r")) && !Token.Contains(TEXT("\n")))
                {
                    Headers.Add(TEXT("Authorization"), TEXT("Bearer ") + Token);
                }
                return Headers;
            };

            const TSharedRef<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe> Transport =
                MakeShared<FGamePlatformTelemetryHttpTransport, ESPMode::ThreadSafe>(
                    BaseUrl,
                    TEXT("/internal/telemetry/v1/batches"),
                    MoveTemp(StaticHeaders),
                    5.0f,
                    256 * 1024,
                    MoveTemp(HeaderProvider));

            FGamePlatformTelemetryRetrySettings Retry;
            Retry.MaxRetries = 4;
            Retry.MaxPendingBatches = 8;
            Retry.MaxRetryAgeSeconds = 30.0f;
            Telemetry->ConfigureSink(
                MakeShared<FGamePlatformTelemetryNetworkSink, ESPMode::ThreadSafe>(Transport, Retry));
        }
    }

    if (UGamePlatformServerLifecycleSubsystem* Lifecycle =
            GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>())
    {
        LifecycleChangedHandle = Lifecycle->OnLifecycleChanged().AddUObject(
            this,
            &UDivineBeastsServerBootstrapSubsystem::HandleLifecycleChanged);
    }
    else
    {
        SetFailed(TEXT("ServerLifecycleUnavailable"));
        return;
    }

    State = EDivineBeastsServerBootstrapState::WaitingForWorld;
    WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
        this,
        &UDivineBeastsServerBootstrapSubsystem::HandleWorldInitialized);

    // GameInstance可能在首个World BeginPlay之后才创建子系统；只复核当前实例，不强制载图。
    ObserveWorld(GetGameInstance()->GetWorld());
}

void UDivineBeastsServerBootstrapSubsystem::Deinitialize()
{
    if (WorldInitializedHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
        WorldInitializedHandle.Reset();
    }
    StopObservingWorld();
    if (UGamePlatformServerLifecycleSubsystem* Lifecycle =
            GetGameInstance()
                ? GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>()
                : nullptr)
    {
        if (LifecycleChangedHandle.IsValid())
        {
            Lifecycle->OnLifecycleChanged().Remove(LifecycleChangedHandle);
        }
    }
    LifecycleChangedHandle.Reset();
    ValidatedWorld.Reset();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGamePlatformTelemetrySubsystem* Telemetry =
                GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>())
        {
            Telemetry->FlushBestEffort();
        }
    }
    Super::Deinitialize();
}

void UDivineBeastsServerBootstrapSubsystem::HandleWorldInitialized(
    UWorld* World,
    const UWorld::InitializationValues)
{
    ObserveWorld(World);
}

void UDivineBeastsServerBootstrapSubsystem::ObserveWorld(UWorld* World)
{
    if (World == nullptr || World->GetGameInstance() != GetGameInstance())
    {
        return;
    }

    if (ObservedWorld.Get() != World)
    {
        StopObservingWorld();
        ObservedWorld = World;
    }

    if (World->HasBegunPlay())
    {
        HandleWorldBeginPlay(World);
        return;
    }

    if (!WorldBeginPlayHandle.IsValid())
    {
        WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(
            this,
            &UDivineBeastsServerBootstrapSubsystem::HandleObservedWorldBeginPlay);
    }
}

void UDivineBeastsServerBootstrapSubsystem::HandleObservedWorldBeginPlay()
{
    UWorld* World = ObservedWorld.Get();
    if (World != nullptr && WorldBeginPlayHandle.IsValid())
    {
        World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
    }
    WorldBeginPlayHandle.Reset();
    HandleWorldBeginPlay(World);
}

void UDivineBeastsServerBootstrapSubsystem::StopObservingWorld()
{
    if (UWorld* World = ObservedWorld.Get();
        World != nullptr && WorldBeginPlayHandle.IsValid())
    {
        World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
    }
    WorldBeginPlayHandle.Reset();
    ObservedWorld.Reset();
}

bool UDivineBeastsServerBootstrapSubsystem::ReportHeartbeat(int32 CurrentPlayers)
{
    if (State != EDivineBeastsServerBootstrapState::Registered &&
        State != EDivineBeastsServerBootstrapState::Ready &&
        State != EDivineBeastsServerBootstrapState::Draining)
    {
        return false;
    }
    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    return Lifecycle != nullptr && Lifecycle->SendHeartbeat(CurrentPlayers);
}

bool UDivineBeastsServerBootstrapSubsystem::BeginDrain()
{
    if (State != EDivineBeastsServerBootstrapState::Registered &&
        State != EDivineBeastsServerBootstrapState::Ready)
    {
        return false;
    }
    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    return Lifecycle != nullptr && Lifecycle->BeginDrain();
}

bool UDivineBeastsServerBootstrapSubsystem::CompleteDrain()
{
    if (State != EDivineBeastsServerBootstrapState::Draining)
    {
        return false;
    }
    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    return Lifecycle != nullptr && Lifecycle->CompleteDrain();
}

void UDivineBeastsServerBootstrapSubsystem::LoadLaunchProfile()
{
    FString RoleName;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ServerRole="), RoleName) ||
        RoleName.IsEmpty())
    {
        SetFailed(TEXT("ServerRoleMissing"));
        return;
    }

    FString Error;
    if (!FDivineBeastsServerRoleProfile::TryLoadForRoleName(
            RoleName,
            ActiveProfile,
            Error))
    {
        SetFailed(TEXT("ServerProfileInvalid"));
        return;
    }

    ActiveExperienceId = ActiveProfile.DefaultExperienceId;
    FString ExperienceOverride;
    if (FParse::Value(FCommandLine::Get(), TEXT("ExperienceId="), ExperienceOverride))
    {
        const FName RequestedExperience(*ExperienceOverride);
        if (!ActiveProfile.AllowedExperienceIds.Contains(RequestedExperience))
        {
            SetFailed(TEXT("ServerExperienceNotAllowed"));
            return;
        }
        ActiveExperienceId = RequestedExperience;
    }

    bHasProfile = true;
    State = EDivineBeastsServerBootstrapState::ProfileLoaded;
    LastErrorCode = NAME_None;
}

void UDivineBeastsServerBootstrapSubsystem::HandleWorldBeginPlay(UWorld* World)
{
    if (!bHasProfile || bWorldValidated || World == nullptr ||
        World->GetGameInstance() != GetGameInstance() ||
        GetGameInstance()->GetWorld() != World ||
        World->GetNetMode() != NM_DedicatedServer ||
        World->WorldType != EWorldType::Game)
    {
        return;
    }
    RegisterValidatedWorld(*World);
}

void UDivineBeastsServerBootstrapSubsystem::RegisterValidatedWorld(UWorld& World)
{
    FString Reason;
    if (!IsConfiguredWorldValid(World, Reason))
    {
        SetFailed(TEXT("ServerWorldOrAssetsInvalid"));
        return;
    }

    FGamePlatformServerInstanceInfo Instance;
    Instance.GameId = FDivineBeastsProjectCatalog::GetGameId().ToString();
    Instance.GameServerId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_ID"));
    Instance.ServerRoleId = ActiveProfile.ServerRoleId.ToString();
    Instance.ExperienceId = ActiveExperienceId.ToString();
    Instance.WorldId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_WORLD_ID"));
    Instance.RegionId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_REGION_ID"));
    Instance.ClusterId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_CLUSTER_ID"));
    Instance.NodeId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_NODE_ID"));
    Instance.BuildVersion = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_BUILD_VERSION"));
    Instance.PublicEndpoint = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_PUBLIC_ENDPOINT"));
    FString CapacityText = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_CAPACITY"));
    FString ProtocolVersionText = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_PROTOCOL_VERSION"));
    if (!FDefaultValueHelper::ParseInt(CapacityText, Instance.Capacity) ||
        (!ProtocolVersionText.IsEmpty() &&
            (!FDefaultValueHelper::ParseInt(ProtocolVersionText, Instance.ProtocolVersion) ||
                Instance.ProtocolVersion < 0)) ||
        !Instance.IsValid())
    {
        SetFailed(TEXT("ServerInstanceEnvironmentInvalid"));
        return;
    }

    if (UGamePlatformTelemetrySubsystem* Telemetry =
            GetGameInstance()->GetSubsystem<UGamePlatformTelemetrySubsystem>())
    {
        Telemetry->SetServerContext(Instance.ServerRoleId, Instance.RegionId, Instance.GameServerId);
        Telemetry->UpdateWorldContext(
            World.GetMapName(),
            Instance.WorldId,
            Instance.ExperienceId,
            FString(),
            FString());

        FGamePlatformTelemetryEvent Started;
        Started.EventName = TEXT("Telemetry.Foundation.ServerStarted");
        Telemetry->RecordEvent(MoveTemp(Started));
    }

    ValidatedWorld = &World;
    bWorldValidated = true;
    State = EDivineBeastsServerBootstrapState::Registering;
    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    if (Lifecycle == nullptr || !Lifecycle->RegisterInstance(Instance))
    {
        SetFailed(TEXT("ServerRegistrationRejected"));
    }
}

bool UDivineBeastsServerBootstrapSubsystem::IsConfiguredWorldValid(
    UWorld& World,
    FString& OutReason) const
{
    if (!bHasProfile || !ActiveProfile.bRequireWorldBeginPlay ||
        World.GetPackage() == nullptr ||
        World.GetPackage()->GetName() != ActiveProfile.WorldPackage)
    {
        OutReason = TEXT("Dedicated Server加载地图与Profile不一致。");
        return false;
    }

    TArray<FString> MissingAssets;
    if (ActiveProfile.bRequireRequiredAssets &&
        !ActiveProfile.FindMissingRequiredAssets(MissingAssets))
    {
        OutReason = TEXT("服务器制品缺少Profile声明的必要地图或资源。");
        return false;
    }
    return true;
}

void UDivineBeastsServerBootstrapSubsystem::HandleLifecycleChanged(
    const FGamePlatformServerLifecycleSnapshot& Snapshot)
{
    switch (Snapshot.State)
    {
    case EGamePlatformServerLifecycleState::Registering:
        State = EDivineBeastsServerBootstrapState::Registering;
        break;
    case EGamePlatformServerLifecycleState::Registered:
        State = EDivineBeastsServerBootstrapState::Registered;
        if (bWorldValidated && ValidatedWorld.IsValid())
        {
            const TWeakObjectPtr<UDivineBeastsServerBootstrapSubsystem> WeakThis(this);
            AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                UDivineBeastsServerBootstrapSubsystem* Self = WeakThis.Get();
                if (!Self || !Self->ValidatedWorld.IsValid() ||
                    Self->State != EDivineBeastsServerBootstrapState::Registered)
                {
                    return;
                }
                FString Reason;
                if (!Self->IsConfiguredWorldValid(*Self->ValidatedWorld.Get(), Reason))
                {
                    Self->SetFailed(TEXT("ServerReadyGateLost"));
                    return;
                }
                UGamePlatformServerLifecycleSubsystem* Lifecycle =
                    Self->GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
                if (Lifecycle == nullptr || !Lifecycle->MarkReady())
                {
                    Self->SetFailed(TEXT("ServerReadyRejected"));
                }
            });
        }
        break;
    case EGamePlatformServerLifecycleState::PublishingReady:
        State = EDivineBeastsServerBootstrapState::Registered;
        break;
    case EGamePlatformServerLifecycleState::Ready:
        State = EDivineBeastsServerBootstrapState::Ready;
        LastErrorCode = NAME_None;
        break;
    case EGamePlatformServerLifecycleState::Draining:
        State = EDivineBeastsServerBootstrapState::Draining;
        break;
    case EGamePlatformServerLifecycleState::Stopped:
        State = EDivineBeastsServerBootstrapState::Stopped;
        break;
    case EGamePlatformServerLifecycleState::Failed:
        SetFailed(Snapshot.ErrorCode.IsNone()
            ? FName(TEXT("ServerLifecycleFailed"))
            : Snapshot.ErrorCode);
        break;
    case EGamePlatformServerLifecycleState::Unregistered:
        break;
    }
}

void UDivineBeastsServerBootstrapSubsystem::SetFailed(FName ErrorCode)
{
    State = EDivineBeastsServerBootstrapState::Failed;
    LastErrorCode = ErrorCode.IsNone()
        ? FName(TEXT("ServerBootstrapFailed"))
        : ErrorCode;
}
