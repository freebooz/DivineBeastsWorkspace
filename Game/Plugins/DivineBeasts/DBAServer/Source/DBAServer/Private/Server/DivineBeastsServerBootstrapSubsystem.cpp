// 第三层Server/Editor组合根：Profile授权当前承载世界，Village复用平台World/Data/Experience与准入链。
// 自有世界委托/启动Timer在失败或退出时撤销；永久世界退休门闩使旧Ready/体验回调不能重新接纳。
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
    // 已退休实例不重新注册全局委托/Telemetry或签发操作；重新承载必须创建新的服务器实例。
    if (bWorldRetired) { return; }
    Super::Initialize(Collection);
    BootstrapOperationId = FGuid::NewGuid();
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
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &ThisClass::HandleWorldCleanup);

    // GameInstance可能在首个World BeginPlay之后才创建子系统；只复核当前实例，不强制载图。
    ObserveWorld(GetGameInstance()->GetWorld());
}

void UDivineBeastsServerBootstrapSubsystem::Deinitialize()
{
    // 即使尚无ValidatedWorld也永久关闭本实例；外部清理/Telemetry通知之前先使所有原操作失效。
    bWorldRetired = true;
    BootstrapOperationId.Invalidate();
    // 先撤本世界的启动Timer，再走完整退出撤销；ValidatedWorld会在Cleanup内清空，不能之后才找Timer所有者。
    if (UWorld* World = ValidatedWorld.Get()) World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
    if (ValidatedWorld.IsValid()) HandleWorldCleanup(ValidatedWorld.Get(), true, true);
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    WorldCleanupHandle.Reset();
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

void UDivineBeastsServerBootstrapSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
    if (!World || (World != ObservedWorld.Get() && World != ValidatedWorld.Get())) return;
    if (World != ValidatedWorld.Get())
    { StopObservingWorld(); return; }
    // 固定角色Profile只授权已校验的承载世界。先本地关闭，控制面的异步排空确认不能成为继续准入的窗口。
    bWorldRetired = true;
    BootstrapOperationId.Invalidate();
    bWorldValidated = false;
    // 世界退出也撤销Main新增体验推进Timer；该Boot不再承载后继地图，已排队回调仍由退休门闩拒绝。
    World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
    ValidatedWorld.Reset();
    StopObservingWorld();
    if (auto* Admission = GetGameInstance()->GetSubsystem<UGamePlatformServerAdmissionSubsystem>())
        Admission->ResetTarget(TEXT("ServerWorldRetired"));
    if (auto* Lifecycle = GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>())
    {
        Lifecycle->StopHeartbeatPump();
        Lifecycle->BeginDrain();
    }
    // 重新承载必须由部署启动新实例并重新校验Profile/Boot；常驻世界内部区域流送不走本路径。
    LastErrorCode = TEXT("ServerWorldRetiredRestartRequired");
    if (State != EDivineBeastsServerBootstrapState::Draining &&
        State != EDivineBeastsServerBootstrapState::Stopped)
        State = EDivineBeastsServerBootstrapState::Failed;
}

void UDivineBeastsServerBootstrapSubsystem::ObserveWorld(UWorld* World)
{
    if (bWorldRetired || World == nullptr || World->GetGameInstance() != GetGameInstance())
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
    // 本地排空意图先关闭Ready/体验队列；平台在途Register可能仍投影Registered，但不能重新开启项目准入。
    State = EDivineBeastsServerBootstrapState::Draining;
    BootstrapOperationId.Invalidate();
    if (UWorld* World = ValidatedWorld.Get()) World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
    if (auto* Admission = GetGameInstance()->GetSubsystem<UGamePlatformServerAdmissionSubsystem>())
        Admission->StopAcceptingAdmissions();
    if (bWorldRetired) { return false; }
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
    if (bWorldRetired || !bHasProfile || bWorldValidated || World == nullptr ||
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
    BootstrapOperationId = FGuid::NewGuid();
    const FGuid OperationId = BootstrapOperationId;
    const TWeakObjectPtr<UWorld> ExpectedWorld(&World);
    const TWeakObjectPtr<UGameInstance> ExpectedInstance(GetGameInstance());
    const FString ExpectedBootId = ServerBootId;
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
    const auto BoundWorldResult = WorldService->InitializeBoundWorld(WorldAsset,Context);
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
    if(!BoundWorldResult.IsSuccess()) {SetFailed(TEXT("ProjectWorldInitializeRejected"));return;}
    FGamePlatformResult Registered;
    Mode->RegisterAdmissionAuthority(Mode,MakeShared<FDivineBeastsWorldAdmissionAuthority>(Admission),Registered);
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
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
    else
    {
        const bool bRegistered = Lifecycle->RegisterInstance(Instance);
        if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
        if (!bRegistered && Lifecycle->GetSnapshot().State != EGamePlatformServerLifecycleState::Failed)
        {
            // 若平台层已同步推进Failed，则保留其更精确的错误码，不用泛化错误覆盖诊断信息。
            SetFailed(TEXT("ServerRegistrationRejected"));
        }
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
    if (bWorldRetired)
    {
        State = Snapshot.State == EGamePlatformServerLifecycleState::Stopped
            ? EDivineBeastsServerBootstrapState::Stopped
            : Snapshot.State == EGamePlatformServerLifecycleState::Draining
                ? EDivineBeastsServerBootstrapState::Draining : EDivineBeastsServerBootstrapState::Failed;
        LastErrorCode = TEXT("ServerWorldRetiredRestartRequired");
        return;
    }
    // 资源失败是项目启动终态；排空回调或迟到的注册完成不能重新发布Ready。
    if (State == EDivineBeastsServerBootstrapState::Failed)
    {
        if (Snapshot.State == EGamePlatformServerLifecycleState::Registered || Snapshot.State == EGamePlatformServerLifecycleState::Ready)
            if (auto* Lifecycle = GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>()) Lifecycle->BeginDrain();
        return;
    }
    // 显式Drain可能在平台Register/Ready在途时排队，Registered/PublishingReady旧通知不能清掉本地排空意图。
    if (State == EDivineBeastsServerBootstrapState::Draining &&
        Snapshot.State != EGamePlatformServerLifecycleState::Draining &&
        Snapshot.State != EGamePlatformServerLifecycleState::Stopped &&
        Snapshot.State != EGamePlatformServerLifecycleState::Failed) { return; }
    switch (Snapshot.State)
    {
    case EGamePlatformServerLifecycleState::Registering:
        State = EDivineBeastsServerBootstrapState::Registering;
        LastErrorCode = Snapshot.ErrorCode;
        break;
    case EGamePlatformServerLifecycleState::Registered:
        BootstrapOperationId = FGuid::NewGuid();
        State = EDivineBeastsServerBootstrapState::Registered;
        LastErrorCode = Snapshot.ErrorCode;
        if (bWorldValidated && ValidatedWorld.IsValid())
        {
            const TWeakObjectPtr<UDivineBeastsServerBootstrapSubsystem> WeakThis(this);
            const FGuid OperationId = BootstrapOperationId;
            const TWeakObjectPtr<UWorld> ExpectedWorld = ValidatedWorld;
            const TWeakObjectPtr<UGameInstance> ExpectedInstance(GetGameInstance());
            const FString ExpectedBootId = ServerBootId;
            const TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> ExpectedLifecycle(
                GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>());
            AsyncTask(ENamedThreads::GameThread, [WeakThis, OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId, ExpectedLifecycle, Snapshot]()
            {
                UDivineBeastsServerBootstrapSubsystem* Self = WeakThis.Get();
                // weakThis和当前Registered不足以区分原排队任务；nonce拒绝同World/Boot的新通知接管。
                if (!Self || !Self->IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId) ||
                    Self->State != EDivineBeastsServerBootstrapState::Registered || !ExpectedLifecycle.IsValid() ||
                    ExpectedInstance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>() != ExpectedLifecycle.Get())
                {
                    return;
                }
                const auto Current = ExpectedLifecycle->GetSnapshot();
                // 平台诊断快照这里只用于拒绝旧通知，不作为授权或替代平台自身异步代次校验。
                if (Current.OperationGeneration != Snapshot.OperationGeneration || Current.State != EGamePlatformServerLifecycleState::Registered ||
                    Current.GameServerId != Snapshot.GameServerId || Current.ExperienceId != Snapshot.ExperienceId) { return; }
                Self->TryPublishReady();
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
        if (auto* Admission = GetGameInstance()->GetSubsystem<UGamePlatformServerAdmissionSubsystem>())
            Admission->StopAcceptingAdmissions();
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
    if (bWorldRetired) { return; }
    BootstrapOperationId.Invalidate();
    State = EDivineBeastsServerBootstrapState::Failed;
    LastErrorCode = ErrorCode.IsNone()
        ? FName(TEXT("ServerBootstrapFailed"))
        : ErrorCode;
    // 先停本地体验推进，再撤销准入Target；Reset可能同步通知消费者，世界退出终态必须优先保留。
    if (UWorld* World = ValidatedWorld.Get()) World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
    UE_LOG(LogTemp, Error, TEXT("[DBA Server] Bootstrap failed: %s"), *LastErrorCode.ToString());
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (auto* Admission = GameInstance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>())
            Admission->ResetTarget(LastErrorCode);
        if (bWorldRetired) { return; }
        if (auto* Lifecycle = GameInstance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>())
        {
            Lifecycle->StopHeartbeatPump();
            // 注册后失败撤销控制面可接纳状态；BeginDrain异步返回不改写上述项目终态。
            const auto LifecycleState = Lifecycle->GetSnapshot().State;
            if (LifecycleState == EGamePlatformServerLifecycleState::Registered ||
                LifecycleState == EGamePlatformServerLifecycleState::Ready) Lifecycle->BeginDrain();
        }
    }
}

void UDivineBeastsServerBootstrapSubsystem::TryPublishReady()
{
    // Registered与体验Active的先后不固定，但世界退休/失败后两种迟到完成都不能重新发布Ready。
    if (bWorldRetired || State != EDivineBeastsServerBootstrapState::Registered || !bWorldValidated || !ValidatedWorld.IsValid()) return;
    const FGuid OperationId = BootstrapOperationId;
    const TWeakObjectPtr<UWorld> ExpectedWorld = ValidatedWorld;
    const TWeakObjectPtr<UGameInstance> ExpectedInstance(GetGameInstance());
    const FString ExpectedBootId = ServerBootId;
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
    FString Reason;
    if (!IsConfiguredWorldValid(*ValidatedWorld.Get(), Reason)) { SetFailed(TEXT("ServerReadyGateLost")); return; }
    // Village出生依赖唯一Experience执行器；Preparing/Failed均不能接纳。其他角色沿用既有Profile门禁。
    if (ActiveProfile.ServerRoleId == TEXT("GameServer.Role.Village"))
    {
        const int32 Players = GetCurrentPlayerCount();
        if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
        if (Players < 0) { return; }
    }
    auto* Lifecycle = GetGameInstance()->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
    if (!Lifecycle) { SetFailed(TEXT("ServerLifecycleUnavailable")); return; }
    // PublishingReady阶段仍投影为Registered，使用平台阶段防止重复提交同一Ready操作。
    if (Lifecycle->GetSnapshot().State != EGamePlatformServerLifecycleState::Registered) return;
    const TWeakObjectPtr<UGamePlatformServerLifecycleSubsystem> ExpectedLifecycle(Lifecycle);
    const auto BeforeReady = Lifecycle->GetSnapshot();
    const bool bReadyAccepted = Lifecycle->MarkReady();
    // PublishingReady原生通知允许退出/Deinitialize/后继操作。先核本栈原scope，旧false不得覆盖退休错误。
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId) ||
        !ExpectedLifecycle.IsValid() || ExpectedInstance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>() != ExpectedLifecycle.Get()) { return; }
    const auto AfterReady = ExpectedLifecycle->GetSnapshot();
    if (!bReadyAccepted && AfterReady.State == EGamePlatformServerLifecycleState::Registered &&
        AfterReady.OperationGeneration == BeforeReady.OperationGeneration)
        SetFailed(TEXT("ServerReadyRejected"));
}

bool UDivineBeastsServerBootstrapSubsystem::IsBootstrapScopeCurrent(FGuid OperationId, TWeakObjectPtr<UWorld> World,
    TWeakObjectPtr<UGameInstance> Instance, const FString& BootId) const
{
    return IsValid(this) && !bWorldRetired && OperationId.IsValid() && BootstrapOperationId == OperationId &&
        World.IsValid() && ValidatedWorld == World && bWorldValidated && !World->bIsTearingDown &&
        Instance.IsValid() && GetGameInstance() == Instance.Get() && World->GetGameInstance() == Instance.Get() &&
        ServerBootId == BootId && State != EDivineBeastsServerBootstrapState::Failed &&
        State != EDivineBeastsServerBootstrapState::Draining && State != EDivineBeastsServerBootstrapState::Stopped;
}

int32 UDivineBeastsServerBootstrapSubsystem::GetCurrentPlayerCount() const
{
    const FGuid OperationId = BootstrapOperationId;
    const TWeakObjectPtr<UWorld> ExpectedWorld = ValidatedWorld;
    const TWeakObjectPtr<UGameInstance> ExpectedInstance(GetGameInstance());
    const FString ExpectedBootId = ServerBootId;
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return -1; }
    const UWorld* World = ValidatedWorld.Get();
    if (World == nullptr || World->GetGameInstance() != GetGameInstance())
    {
        return -1;
    }
    AGameModeBase* GameMode = World->GetAuthGameMode();
    if(ActiveProfile.ServerRoleId!=TEXT("GameServer.Role.Village"))
    {
        const int32 Players = GameMode ? GameMode->GetNumPlayers() : 0;
        return IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId) ? Players : -1;
    }
    auto* Mode=Cast<ADivineBeastsWorldGameMode>(GameMode);
    const auto* Experience=Mode?Mode->GetExperienceComponent():nullptr;
    if (!Experience || !Experience->GetExperienceSnapshot().IsServerActive()) { return -1; }
    const int32 Players = Mode->GetNumPlayers();
    return IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId) ? Players : -1;
}
void UDivineBeastsServerBootstrapSubsystem::AdvanceGameplayBootstrap()
{
    auto* World=ValidatedWorld.Get(); if(!World)return;
    const FGuid OperationId = BootstrapOperationId;
    const TWeakObjectPtr<UWorld> ExpectedWorld = ValidatedWorld;
    const TWeakObjectPtr<UGameInstance> ExpectedInstance(GetGameInstance());
    const FString ExpectedBootId = ServerBootId;
    if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
    // Timer可能已排队；失败、排空或退出后只撤本世界Timer，不再启动体验或尝试Ready。
    if (bWorldRetired || State == EDivineBeastsServerBootstrapState::Failed ||
        State == EDivineBeastsServerBootstrapState::Draining || State == EDivineBeastsServerBootstrapState::Stopped)
    { World->GetTimerManager().ClearTimer(GameplayBootstrapTimer); return; }
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
        // 真正外部边界为内部Data AcquireDefinition；返回先复核，旧失败不能调用SetFailed或清后继Timer。
        if (!IsBootstrapScopeCurrent(OperationId, ExpectedWorld, ExpectedInstance, ExpectedBootId)) { return; }
        if(!Result.IsSuccess()){World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);SetFailed(Result.Code);return;}
        // BeginExperience内部Data调用可同步重入；只继续读取原承载世界，不在退出/失败后的旧Timer栈发布Ready。
        if (bWorldRetired || ValidatedWorld.Get() != World || State == EDivineBeastsServerBootstrapState::Failed) return;
    }
    const auto Snapshot = Experience->GetExperienceSnapshot();
    if (Snapshot.Stage == EGamePlatformExperienceStage::Failed)
    { SetFailed(Snapshot.FailureCode.IsNone() ? FName(TEXT("WorldExperienceFailed")) : Snapshot.FailureCode); return; }
    if(Snapshot.IsServerActive())
    {
        World->GetTimerManager().ClearTimer(GameplayBootstrapTimer);
        // 注册完成和体验激活先后顺序不固定，后到的一方再次进入同一门禁。
        TryPublishReady();
    }
}
