#include "Bootstrap/DBAFoundationCoordinator.h"
#include "Bootstrap/DBAFoundationHUD.h"
#include "Bootstrap/DBAFoundationPolicy.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Bootstrap/Nodes/DBAFoundationNode.h"
#include "Bootstrap/Nodes/DBALoadingFlowNode.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Modules/ModuleManager.h"
#include "Types/GamePlatformVersion.h"
#include "Bootstrap/Online/DBAFoundationOnlineContext.h"
#include "Bootstrap/Online/DBAFoundationOnlineNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAFoundation, Log, All);

void UDBAFoundationCoordinator::Initialize(UGameInstance& Owner)
{
    Shutdown();
#if !UE_BUILD_SHIPPING
    bOnlineIntegration = FParse::Param(FCommandLine::Get(), TEXT("FoundationOnlineIntegration"));
    const bool bStandalone = FParse::Param(FCommandLine::Get(), TEXT("FoundationStandalone"));
    if (bOnlineIntegration && bStandalone)
    {
        Diagnostics = TEXT("基础验证入口冲突：OnlineIntegration与Standalone必须互斥");
        return;
    }
    if (DBA::Foundation::ResolveMode(!UE_BUILD_SHIPPING,
        bStandalone || bOnlineIntegration,
        IsRunningCommandlet(), IsRunningDedicatedServer()) == DBA::Foundation::EMode::Disabled) { return; }
    OwnerInstance = &Owner;
    FGuid ParsedRunId;
    if (!FParse::Value(FCommandLine::Get(), TEXT("FoundationRunId="), RunId) || !FGuid::Parse(RunId, ParsedRunId))
    {
        Diagnostics = TEXT("基础工程开发验证：缺少有效 FoundationRunId，拒绝启动");
        UE_LOG(LogDBAFoundation, Error, TEXT("%s"), *Diagnostics);
        return;
    }
    bStopping = false;
    bStartAttempted = false;
    LastResult = {};
    DiscoveryDeadlineSeconds = FPlatformTime::Seconds() + 60.0;
    Diagnostics = TEXT("基础工程开发验证：等待本实例世界就绪");
    // 低频实例级轮询等待可操作世界；切图不借用旧世界计时器，不假造固定延时成功。
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDBAFoundationCoordinator::Tick), 0.1f);
#endif
}

bool UDBAFoundationCoordinator::Tick(float)
{
    UGameInstance* Instance = OwnerInstance.Get();
    if (!Instance) { return false; }
    // 世界尚未创建或尚未BeginPlay同样受实例启动截止约束；禁止无限等待绕过超时。
    if (DBA::Foundation::StartupDeadlineExceeded(bStartAttempted, bStopping,
        FPlatformTime::Seconds(), DiscoveryDeadlineSeconds))
    {
        bStartAttempted = true;
        LastResult = FGamePlatformResult::Failure(TEXT("DiscoveryTimeout"), TEXT("等待本实例世界、资产发现或本地观察者超时"));
        Diagnostics = LastResult.Message;
        UE_LOG(LogDBAFoundation, Warning, TEXT("FoundationFailed RunId=%s Code=DiscoveryTimeout"), *RunId);
    }
    UWorld* World = Instance->GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || World->GetGameInstance() != Instance) { return true; }
    const FString Map = World->GetOutermost()->GetName();
    if (World->GetNetMode() == NM_DedicatedServer)
    {
        if (ReportedWorld.Get() != World)
        {
            UE_LOG(LogDBAFoundation, Display, TEXT("FoundationServerReady RunId=%s Map=%s"), *RunId, *Map);
            ReportedWorld = World;
        }
        Diagnostics = TEXT("基础专用服务器：世界就绪，不创建玩家HUD或主流程");
        return true;
    }
    bool bHasLocalController = false;
    for (ULocalPlayer* Player : Instance->GetLocalPlayers())
    {
        APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
        if (!Controller || !Controller->IsLocalController()) { continue; }
        bHasLocalController = true;
        if (!Cast<ADBAFoundationHUD>(Controller->GetHUD())) { Controller->ClientSetHUD(ADBAFoundationHUD::StaticClass()); }
    }
    if (!bStartAttempted && !bStopping)
    {
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        if (bHasLocalController && !Registry.IsLoadingAssets()) { StartDevelopmentFlow(); }
        else if (FPlatformTime::Seconds() >= DiscoveryDeadlineSeconds)
        {
            bStartAttempted = true;
            LastResult = FGamePlatformResult::Failure(TEXT("DiscoveryTimeout"), TEXT("等待真实资产发现或本地观察者超时；不会回退到虚假测试结果"));
        }
    }
    const auto Snapshot = FlowService.IsValid() ? FlowService->GetSnapshot() : FGamePlatformFlowSnapshot{};
    int32 ProbeValue = 0;
    const bool bProbeReady = ReadProbe(ProbeValue);
    FGamePlatformId ProbeId;
    FGamePlatformVersion Version;
    const bool bCoreValid = FGamePlatformId::TryParse(TEXT("Foundation.Probe@1"), ProbeId) &&
        FGamePlatformVersion::TryParse(TEXT("0.1.0"), Version);
    Diagnostics = FString::Printf(TEXT("基础工程开发验证\n地图：%s\n进程验证：%s\n流程：%llu / 节点：%s / 代次：%llu / 状态：%d\n核心：%s / %s\n探针：%s / 值：%d / 租约：%s\n错误：%s %s"),
        *Map, *RunId, Snapshot.Handle.RunId, *Snapshot.NodeId.ToString(), Snapshot.NodeGeneration, static_cast<int32>(Snapshot.State),
        bCoreValid ? *ProbeId.ToString() : TEXT("身份解析失败"), *Version.ToString(),
        bProbeReady ? TEXT("真实资产可读") : TEXT("未就绪"), ProbeValue, *ProbeLease.LeaseId.ToString(),
        *LastResult.Code.ToString(), *LastResult.Message);
    if (OnlineContext) { Diagnostics += TEXT("\n") + OnlineContext->GetDiagnostics(); }
    if (bHasLocalController && ReportedWorld.Get() != World)
    {
        UE_LOG(LogDBAFoundation, Display, TEXT("FoundationHostReady RunId=%s Map=%s"), *RunId, *Map);
        ReportedWorld = World;
    }
    return true;
}

void UDBAFoundationCoordinator::Shutdown()
{
    bStopping = true;
    CancelDevelopmentFlow();
    if (auto* Flow = FlowService.Get())
    {
        Flow->OnFinished().Remove(FinishedHandle);
        for (const auto& Handle : FactoryHandles) { FGamePlatformResult Result; Flow->UnregisterNodeFactory(Handle, Result); }
    }
    FactoryHandles.Reset();
    FinishedHandle.Reset();
    FlowService.Reset();
    if (TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); TickerHandle.Reset(); }
    OwnerInstance.Reset();
    ReportedWorld.Reset();
    Diagnostics = TEXT("基础工程开发验证未启用或已关闭");
    bOnlineIntegration = false;
}

void UDBAFoundationCoordinator::StartDevelopmentFlow()
{
    bStartAttempted = true;
    UGameInstance* Instance = OwnerInstance.Get();
    LastResult = ValidateConfiguration();
    if (!Instance || !LastResult.IsSuccess()) { return; }
    auto* Flow = Instance->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
    auto* Data = IGamePlatformDataService::Get(*Instance);
    if (!Flow || !Data)
    {
        LastResult = FGamePlatformResult::Failure(TEXT("FoundationServicesMissing"), TEXT("真实流程或数据服务未初始化"));
        return;
    }
    FlowService = Flow;
    if (FactoryHandles.IsEmpty())
    {
        const TPair<const TCHAR*, EDBAFoundationOperation> Operations[] = {
            {TEXT("Boot"), EDBAFoundationOperation::Boot},
            {TEXT("ValidateConfiguration"), EDBAFoundationOperation::ValidateConfiguration},
            {TEXT("LoadProbeDefinition"), EDBAFoundationOperation::LoadProbeDefinition},
            {TEXT("EnterSandbox"), EDBAFoundationOperation::EnterSandbox},
            {TEXT("Ready"), EDBAFoundationOperation::Ready}};
        TWeakObjectPtr<UDBAFoundationCoordinator> WeakThis(this);
        for (const auto& Pair : Operations)
        {
            const auto Operation = Pair.Value;
            const auto Handle = Flow->RegisterNodeFactory(FName(Pair.Key),
                [WeakThis, Operation](UGameInstance& Owner) -> UGamePlatformFlowNode*
                {
                    auto* Root = WeakThis.Get();
                    if (!Root || Root->OwnerInstance.Get() != &Owner || Root->bStopping) { return nullptr; }
                    if (Operation == EDBAFoundationOperation::Ready)
                    {
                        auto* LoadingNode = NewObject<UDBALoadingFlowNode>(&Owner);
                        LoadingNode->Configure(*Root);
                        return LoadingNode;
                    }
                    auto* Node = NewObject<UDBAFoundationNode>(&Owner);
                    Node->Configure(Operation, *Root);
                    return Node;
                }, LastResult);
            if (!LastResult.IsSuccess())
            {
                for (const auto& Added : FactoryHandles) { FGamePlatformResult Ignored; Flow->UnregisterNodeFactory(Added, Ignored); }
                FactoryHandles.Reset();
                return;
            }
            FactoryHandles.Add(Handle);
        }
        if (bOnlineIntegration)
        {
            const TPair<const TCHAR*, EDBAOnlineOperation> OnlineOperations[] = {
                {TEXT("OnlineValidateConfiguration"), EDBAOnlineOperation::ValidateConfiguration},
                {TEXT("OnlineProbeService"), EDBAOnlineOperation::ProbeService},
                {TEXT("OnlineLogin"), EDBAOnlineOperation::Login},
                {TEXT("OnlineReadProfile"), EDBAOnlineOperation::ReadProfile},
                {TEXT("OnlineReady"), EDBAOnlineOperation::Ready}};
            for (const auto& Pair : OnlineOperations)
            {
                const auto Operation = Pair.Value;
                const auto Handle = Flow->RegisterNodeFactory(FName(Pair.Key),
                    [WeakThis, Operation](UGameInstance& Owner) -> UGamePlatformFlowNode*
                    {
                        auto* Root = WeakThis.Get();
                        if (!Root || Root->OwnerInstance.Get() != &Owner || Root->bStopping) { return nullptr; }
                        auto* Node = NewObject<UDBAFoundationOnlineNode>(&Owner);
                        Node->Configure(Operation, *Root);
                        return Node;
                    }, LastResult);
                if (!LastResult.IsSuccess())
                {
                    for (const auto& Added : FactoryHandles) { FGamePlatformResult Ignored; Flow->UnregisterNodeFactory(Added, Ignored); }
                    FactoryHandles.Reset();
                    return;
                }
                FactoryHandles.Add(Handle);
            }
        }
        FinishedHandle = Flow->OnFinished().AddUObject(this, &UDBAFoundationCoordinator::OnFlowFinished);
    }
    const uint64 Generation = ++RequestGeneration;
    TWeakObjectPtr<UDBAFoundationCoordinator> WeakThis(this);
    FGamePlatformId FlowId;
    if (!FGamePlatformId::TryParse(bOnlineIntegration ? TEXT("foundation.onlineflow@1") : TEXT("foundation.flow@1"), FlowId))
    {
        LastResult = FGamePlatformResult::Failure(TEXT("FlowIdentityInvalid"), TEXT("基础流程逻辑身份解析失败"));
        return;
    }
    FlowLease = Data->AcquireDefinition(FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*FlowId.ToString())),
        UGamePlatformFlowDefinition::StaticClass(), {}, EGamePlatformDataLifetime::Instance, this,
        [WeakThis, Generation](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
        {
            auto* Self = WeakThis.Get();
            if (!Self || Self->bStopping || Self->RequestGeneration != Generation) { return; }
            Self->LastResult = Result;
            if (!Result.IsSuccess()) { Self->ReleaseDataLeases(); return; }
            auto* Service = Self->FlowService.Get();
            if (!Service)
            {
                Self->LastResult = FGamePlatformResult::Failure(TEXT("FlowServiceExpired"), TEXT("定义加载期间流程服务已关闭"));
                Self->ReleaseDataLeases();
                return;
            }
            Self->ActiveFlow = Service->StartFlow(Self->FlowLease, Self, Self->LastResult);
            if (!Self->ActiveFlow.IsValid()) { Self->ReleaseDataLeases(); }
        }, LastResult);
}

void UDBAFoundationCoordinator::OnFlowFinished(const FGamePlatformFlowSnapshot& Snapshot)
{
    if (Snapshot.Handle.ScopeId != ActiveFlow.ScopeId || Snapshot.Handle.RunId != ActiveFlow.RunId) { return; }
    if (Snapshot.State == EGamePlatformFlowState::Succeeded && IsFoundationReady() &&
        (!bOnlineIntegration || (OnlineContext && OnlineContext->IsAuthenticatedWithProfile())))
    {
        LastResult = FGamePlatformResult::Success();
        int32 Value = 0;
        ReadProbe(Value);
        if (bOnlineIntegration) { OnlineContext->OnFoundationReady(); }
        else { UE_LOG(LogDBAFoundation, Display, TEXT("FoundationReady RunId=%s FlowRun=%llu ProbeValue=%d"), *RunId, Snapshot.Handle.RunId, Value); }
        // 保留ProbeLease供只读HUD使用，直到取消、重试或实例关闭；没有借用流程已释放的定义指针。
    }
    else
    {
        LastResult = Snapshot.State == EGamePlatformFlowState::Cancelled ? FGamePlatformResult::Cancelled(TEXT("开发流程已取消")) :
            FGamePlatformResult::Failure(Snapshot.ErrorCode.IsNone() ? FName(TEXT("ReadinessBarrierFailed")) : Snapshot.ErrorCode,
                Snapshot.ErrorMessage.IsEmpty() ? TEXT("流程终止时真实世界与数据屏障未通过") : Snapshot.ErrorMessage);
        ReleaseDataLeases();
        if (OnlineContext) { OnlineContext->Shutdown(); OnlineContext = nullptr; }
        UE_LOG(LogDBAFoundation, Warning, TEXT("FoundationFailed RunId=%s Code=%s Message=%s"), *RunId, *LastResult.Code.ToString(), *LastResult.Message);
    }
}

void UDBAFoundationCoordinator::ReleaseDataLeases()
{
    if (auto* Instance = OwnerInstance.Get())
    {
        if (auto* Data = IGamePlatformDataService::Get(*Instance))
        {
            if (FlowLease.IsValid()) { Data->ReleaseDefinition(FlowLease); }
            if (ProbeLease.IsValid()) { Data->ReleaseDefinition(ProbeLease); }
        }
    }
    FlowLease = {};
    ProbeLease = {};
}

void UDBAFoundationCoordinator::CancelDevelopmentFlow()
{
    ++RequestGeneration;
    AcceptedSandboxWorld.Reset();
    AcceptedTravelOperation.Reset();
    AcceptedTravelFlow = {};
    bStartAttempted = true;
    if (auto* Flow = FlowService.Get()) { Flow->Cancel(ActiveFlow); }
    ActiveFlow = {};
    if (OnlineContext) { OnlineContext->Shutdown(); OnlineContext = nullptr; }
    ReleaseDataLeases();
    LastResult = FGamePlatformResult::Cancelled(TEXT("开发流程已取消；重试须显式创建新运行"));
}

void UDBAFoundationCoordinator::RetryDevelopmentFlow()
{
    if (bStopping || !OwnerInstance.IsValid()) { return; }
    CancelDevelopmentFlow();
    DiscoveryDeadlineSeconds = FPlatformTime::Seconds() + 60.0;
    bStartAttempted = false; // 下一次Tick等实际发现与世界就绪，不在取消回调栈内重入Start。
}

FGamePlatformResult UDBAFoundationCoordinator::InitializeOnlineContext()
{
    check(IsInGameThread());
    auto* Instance = OwnerInstance.Get();
    if (!bOnlineIntegration || bStopping || !Instance)
    { return FGamePlatformResult::Failure(TEXT("OnlineEntryDisabled"), TEXT("本实例未明确启用在线开发入口")); }
    if (OnlineContext)
    { return FGamePlatformResult::Failure(TEXT("OnlineContextAlreadyExists"), TEXT("重复在线初始化需先显式取消并重试")); }
    OnlineContext = NewObject<UDBAFoundationOnlineContext>(this);
    const auto Result = OnlineContext->Initialize(*Instance, RunId);
    if (!Result.IsSuccess()) { OnlineContext->Shutdown(); OnlineContext = nullptr; }
    return Result;
}
