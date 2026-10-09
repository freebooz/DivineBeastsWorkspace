#include "Server/DivineBeastsServerBootstrapSubsystem.h"

#include "Identity/DivineBeastsProjectCatalog.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Sinks/GamePlatformTelemetryNetworkSink.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Transport/GamePlatformTelemetryTransport.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

namespace
{
/** 当前服务器实例的可信准入读取器；每次出生和Active动作均重查真实连接，旧Epoch拒绝。 */
class FDivineBeastsWorldAdmissionAuthority final : public IGamePlatformGameplayAdmissionAuthority
{
    TWeakObjectPtr<UGamePlatformServerAdmissionSubsystem> Source;
public:
    explicit FDivineBeastsWorldAdmissionAuthority(UGamePlatformServerAdmissionSubsystem* S):Source(S){}
    virtual FGamePlatformResult ValidateCurrentAdmission(const APlayerController& C,const FGamePlatformVerifiedPlayerContext& V) const override
    {
        FGamePlatformServerVerifiedAdmission A;
        FGamePlatformId Experience;
        if(!Source.IsValid() || !Source->GetVerifiedAdmission(C,A) || !A.IsStructurallyValid()
            || A.AuthorityUntil<=FDateTime::UtcNow() || A.AdmissionId!=V.AdmissionId || A.ConnectionGeneration!=V.ConnectionGeneration
            || A.SessionEpoch!=V.SessionEpoch || A.AssignmentId!=V.AssignmentId || A.ServerInstanceId!=V.ServerInstanceId
            || !FGamePlatformId::TryParse(A.ExperienceId+TEXT("@1"),Experience) || Experience!=V.ExperienceId)
            return FGamePlatformResult::Failure(TEXT("WorldAdmissionStale"),TEXT("角色准入与当前真实连接不一致"));
        return FGamePlatformResult::Success();
    }
};
}

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

    // Boot身份只在服务器进程/当前GameInstance启动时确定一次。
    // 生产部署可显式注入；本地/开发环境缺失时生成随机GUID，绝不使用固定默认值。
    ServerBootId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_BOOT_ID"));
    if (ServerBootId.IsEmpty())
    {
        ServerBootId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    }

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
        Lifecycle->StopHeartbeatPump();
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
    Instance.ServerBootId = ServerBootId;
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

    // 只有服务器自身完成环境身份与世界校验后，才允许配置准入Target。
    UGamePlatformServerAdmissionSubsystem* Admission =
        GetGameInstance()->GetSubsystem<UGamePlatformServerAdmissionSubsystem>();
    if (Admission == nullptr)
    {
        SetFailed(TEXT("ServerAdmissionUnavailable"));
        return;
    }

    FGamePlatformServerAdmissionTarget AdmissionTarget;
    AdmissionTarget.GameServerId = Instance.GameServerId;
    AdmissionTarget.ServerBootId = Instance.ServerBootId;
    AdmissionTarget.WorldId = Instance.WorldId;
    AdmissionTarget.ExperienceId = Instance.ExperienceId;
    AdmissionTarget.ProtocolVersion = LexToString(Instance.ProtocolVersion);
    // BootId承担跨进程防旧；该本地代次承担同一进程内Target重配栅栏。
    AdmissionTarget.ServerStartGeneration = 1;
    if (!Admission->ConfigureTarget(AdmissionTarget))
    {
        SetFailed(TEXT("ServerAdmissionTargetRejected"));
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
    if(Instance.ServerRoleId==TEXT("GameServer.Role.Village"))
    {
    // 已核对Profile、真实地图和服务器Target后，桥接到平台World/Data与Gameplay唯一运行实例。
    auto* Mode=World.GetAuthGameMode<ADivineBeastsWorldGameMode>();
    auto* WorldService=IGamePlatformWorldService::Get(World);
    if(!Mode || !WorldService){SetFailed(TEXT("ProjectWorldGameModeMissing"));return;}
    FGamePlatformWorldContext Context;
    if(!FGamePlatformId::TryParse(Instance.WorldId,Context.WorldId)
        || !FGamePlatformId::TryParse(Instance.ExperienceId+TEXT("@1"),Context.ExperienceId))
    {SetFailed(TEXT("ProjectWorldIdentityInvalid"));return;}
    Context.ServerInstanceId=Instance.GameServerId; Context.ServerStartGeneration=1;
    Context.ServerRole=TEXT("Village"); Context.AuthorityKind=EGamePlatformWorldAuthority::SessionProjection;
    FGamePlatformVersion::TryParse(Instance.BuildVersion,Context.BuildVersion);
    const FPrimaryAssetId WorldAsset(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*Instance.WorldId));
    if(!WorldService->InitializeBoundWorld(WorldAsset,Context).IsSuccess()) {SetFailed(TEXT("ProjectWorldInitializeRejected"));return;}
    FGamePlatformResult Registered;
    Mode->RegisterAdmissionAuthority(Mode,MakeShared<FDivineBeastsWorldAdmissionAuthority>(Admission),Registered);
    if(!Registered.IsSuccess()){SetFailed(Registered.Code);return;}
    const TWeakObjectPtr<ADivineBeastsWorldGameMode> WeakMode(Mode);
    Admission->OnAdmissionChanged().AddWeakLambda(Mode,[WeakMode](const APlayerController* Controller,const FGamePlatformServerVerifiedAdmission& A,bool bAccepted)
    {
        auto* Current=WeakMode.Get(); if(!Current || !Controller || Controller->GetWorld()!=Current->GetWorld())return;
        auto& C=*const_cast<APlayerController*>(Controller);
        if(!bAccepted){Current->RevokeVerifiedAdmission(C,{A.AdmissionId,A.ConnectionGeneration,A.SessionEpoch});return;}
        FGamePlatformVerifiedPlayerContext V;
        V.AdmissionId=A.AdmissionId; V.AssignmentId=A.AssignmentId; V.ServerInstanceId=A.ServerInstanceId;
        V.ServerStartGeneration=1; V.ConnectionGeneration=A.ConnectionGeneration; V.SessionEpoch=A.SessionEpoch;
        FGamePlatformId::TryCreate(TEXT("divinebeasts.participant"),TEXT("player_")+A.AdmissionId.ToString(EGuidFormats::Digits),1,V.ParticipantId);
        FGamePlatformId::TryParse(A.ExperienceId+TEXT("@1"),V.ExperienceId);
        const auto Result=Current->SubmitVerifiedAdmission(C,V);
        if(!Result.IsSuccess())UE_LOG(LogTemp,Error,TEXT("World admission bridge rejected: %s"),*Result.Code.ToString());
    });
    GameplayBootstrapDeadline=FPlatformTime::Seconds()+30;
    World.GetTimerManager().SetTimer(GameplayBootstrapTimer,this,&ThisClass::AdvanceGameplayBootstrap,0.1f,true);
    }
    State = EDivineBeastsServerBootstrapState::Registering;
    UGamePlatformServerLifecycleSubsystem* Lifecycle =
        GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    if (Lifecycle == nullptr)
    {
        SetFailed(TEXT("ServerLifecycleUnavailable"));
    }
    else if (!Lifecycle->RegisterInstance(Instance) &&
        Lifecycle->GetSnapshot().State != EGamePlatformServerLifecycleState::Failed)
    {
        // 若平台层已同步推进Failed，则保留其更精确的错误码，不用泛化错误覆盖诊断信息。
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
        LastErrorCode = Snapshot.ErrorCode;
        break;
    case EGamePlatformServerLifecycleState::Registered:
        State = EDivineBeastsServerBootstrapState::Registered;
        LastErrorCode = Snapshot.ErrorCode;
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
                if (Lifecycle == nullptr)
                {
                    Self->SetFailed(TEXT("ServerLifecycleUnavailable"));
                }
                else if (!Lifecycle->MarkReady() &&
                    Lifecycle->GetSnapshot().State != EGamePlatformServerLifecycleState::Failed)
                {
                    // 平台层若已给出永久失败原因，不再用项目层通用错误覆盖。
                    Self->SetFailed(TEXT("ServerReadyRejected"));
                }
            });
        }
        break;
    case EGamePlatformServerLifecycleState::PublishingReady:
        State = EDivineBeastsServerBootstrapState::Registered;
        LastErrorCode = Snapshot.ErrorCode;
        break;
    case EGamePlatformServerLifecycleState::Ready:
        State = EDivineBeastsServerBootstrapState::Ready;
        LastErrorCode = Snapshot.ErrorCode;
        if (UGamePlatformServerLifecycleSubsystem* Lifecycle =
                GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>())
        {
            const TWeakObjectPtr<UDivineBeastsServerBootstrapSubsystem> WeakThis(this);
            if (!Lifecycle->StartHeartbeatPump(
                    [WeakThis]()
                    {
                        const UDivineBeastsServerBootstrapSubsystem* Self = WeakThis.Get();
                        return Self ? Self->GetCurrentPlayerCount() : -1;
                    }))
            {
                SetFailed(TEXT("ServerHeartbeatPumpStartFailed"));
            }
        }
        else
        {
            SetFailed(TEXT("ServerLifecycleUnavailable"));
        }
        break;
    case EGamePlatformServerLifecycleState::Draining:
        State = EDivineBeastsServerBootstrapState::Draining;
        LastErrorCode = Snapshot.ErrorCode;
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

int32 UDivineBeastsServerBootstrapSubsystem::GetCurrentPlayerCount() const
{
    const UWorld* World = ValidatedWorld.Get();
    if (World == nullptr || World->GetGameInstance() != GetGameInstance())
    {
        return -1;
    }
    AGameModeBase* GameMode = World->GetAuthGameMode();
    if(ActiveProfile.ServerRoleId!=TEXT("GameServer.Role.Village"))return GameMode?GameMode->GetNumPlayers():0;
    auto* Mode=Cast<ADivineBeastsWorldGameMode>(GameMode);
    const auto* Experience=Mode?Mode->GetExperienceComponent():nullptr;
    return Experience && Experience->GetExperienceSnapshot().IsServerActive() ? Mode->GetNumPlayers() : -1;
}
void UDivineBeastsServerBootstrapSubsystem::AdvanceGameplayBootstrap()
{
    auto* World=ValidatedWorld.Get(); if(!World)return;
    auto* Mode=World->GetAuthGameMode<ADivineBeastsWorldGameMode>();
    auto* Service=IGamePlatformWorldService::Get(*World);
    if(!Mode || !Service || FPlatformTime::Seconds()>GameplayBootstrapDeadline)
    {World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);SetFailed(TEXT("WorldGameplayBootstrapTimeout"));return;}
    if(Service->GetReadiness().Context.ReadinessState!=EGamePlatformWorldReadiness::Ready)return;
    auto* Experience=Mode->GetExperienceComponent();
    if(!Experience)return;
    if(Experience->GetExperienceSnapshot().Stage==EGamePlatformExperienceStage::Unassigned)
    {
        const FPrimaryAssetId Id(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*(ActiveExperienceId.ToString()+TEXT("@1"))));
        const auto Result=Experience->BeginExperience(Id);
        if(!Result.IsSuccess()){World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);SetFailed(Result.Code);}
    }
    if(Experience->GetExperienceSnapshot().IsServerActive())World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
}
