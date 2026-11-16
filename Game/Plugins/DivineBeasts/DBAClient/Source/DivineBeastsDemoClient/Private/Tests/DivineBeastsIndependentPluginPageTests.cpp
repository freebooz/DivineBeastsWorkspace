// 独立插件页安全回归：真实VM/目录事件/页面对象，不加载资产、不联网、不代替人工签收。
// 捕获缺少固定身份防护、旧订阅回调覆盖新页及入口绕过显式资格的回归。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsDemoHubScreen.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UObject/Package.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 仅测试的Data故障边界：签发并记录自己的完整有效租约，再对指定租约注入撤销失败。
 * 正常平台对正确租约释放幂等，故不能用无效句柄或篡改身份冒充有效撤销失败。
 * 此替身不加载资产，不注册为生产Data服务，断言对象为World真实清理后所有权及故障保留结果。
 */
class FDemoPageLeaseFaultService final : public IGamePlatformDataService
{
public:
    FGuid Scope = FGuid::NewGuid();
    TMap<FGuid, FGamePlatformDataLease> Issued;
    TSet<FGuid> RejectRelease;
    TArray<FGuid> ReleaseAttempts;
    FGamePlatformDataLease Issue(const TCHAR* Path)
    {
        FGamePlatformDataLease Lease;
        Lease.ScopeId = Scope; Lease.LeaseId = FGuid::NewGuid(); Lease.Generation = 1;
        Lease.IssuerProof = FGuid::NewGuid(); Lease.ResourcePaths.Add(FSoftObjectPath(Path));
        Lease.RequestState = EGamePlatformDataRequestState::Succeeded;
        Issued.Add(Lease.LeaseId, Lease);
        return Lease;
    }
    virtual FGamePlatformResult ReleaseResources(const FGamePlatformDataLease& Lease) override
    {
        const auto* Original = Issued.Find(Lease.LeaseId);
        if (!Original || Original->ScopeId != Lease.ScopeId || Original->IssuerProof != Lease.IssuerProof ||
            Original->Generation != Lease.Generation || Original->ResourcePaths != Lease.ResourcePaths)
            return FGamePlatformResult::Failure(TEXT("Test.LeaseOwnership"), TEXT("测试租约身份不属于本签发服务。"));
        ReleaseAttempts.Add(Lease.LeaseId);
        if (RejectRelease.Contains(Lease.LeaseId))
            return FGamePlatformResult::Failure(TEXT("Test.ReleaseFault"), TEXT("有效租约撤销故障"));
        Issued.Remove(Lease.LeaseId); return FGamePlatformResult::Success();
    }
    virtual FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId&, TSubclassOf<UGamePlatformDefinitionBase>,
        const TArray<FName>&, EGamePlatformDataLifetime, TWeakObjectPtr<UObject>, FGamePlatformDataCompletion, FGamePlatformResult& Out) override
    { Out = FGamePlatformResult::Unsupported(TEXT("Test.NoAssetLoad"), TEXT("故障边界不加载资产。")); return {}; }
    virtual FGamePlatformDataLease AcquireResources(const TArray<FSoftObjectPath>&, EGamePlatformDataLifetime,
        TWeakObjectPtr<UObject>, FGamePlatformDataCompletion, FGamePlatformResult& Out) override
    { Out = FGamePlatformResult::Unsupported(TEXT("Test.NoAssetLoad"), TEXT("故障边界不加载资产。")); return {}; }
    virtual const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease&) const override { return nullptr; }
    virtual FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease&) override
    { return FGamePlatformResult::Unsupported(TEXT("Test.NoDefinition"), TEXT("测试只拥有普通资源租约。")); }
    virtual EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const override
    { return Issued.Contains(Lease.LeaseId) ? EGamePlatformDataRequestState::Succeeded : EGamePlatformDataRequestState::Released; }
    virtual FGamePlatformDataDiagnostics GetDiagnostics() const override
    { FGamePlatformDataDiagnostics Out; Out.ScopeId = Scope; Out.ActiveLeases = Issued.Num(); return Out; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsIndependentPluginPageTest,
    "DivineBeasts.PluginDemos.IndependentPluginPage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsIndependentPluginPageTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) return false;
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UDivineBeastsDemoCatalogSubsystem> Catalog(NewObject<UDivineBeastsDemoCatalogSubsystem>(Instance.Get()));
    Catalog->Plugins.AddDefaulted_GetRef().PluginId = TEXT("GamePlatformCore");
    Catalog->Plugins.AddDefaulted_GetRef().PluginId = TEXT("GamePlatformUI");
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>());
    TestFalse(TEXT("非法插件路径不能配置为固定身份"), Model->ConfigurePluginContext(TEXT("/Game/Other")));
    TestTrue(TEXT("固定身份配置先于目录绑定"), Model->ConfigurePluginContext(TEXT("GamePlatformCore")));
    Model->BindCatalog(Catalog.Get()); Model->BeginPage();
    FDivineBeastsDemoFilter Search; Search.SearchText = TEXT("GamePlatformUI"); Search.Layer = TEXT("Project");
    Model->SetFilter(Search);
    auto Snapshot = Model->GetMenuSnapshot();
    TestEqual(TEXT("搜索/层筛选不能将固定页换到别的插件"), Snapshot.Plugins.Num(), 1);
    TestEqual(TEXT("固定页仍显示其原PluginId"), Snapshot.SelectedPluginId, FName(TEXT("GamePlatformCore")));
    TestFalse(TEXT("拒绝跨插件选择"), Model->SelectPlugin(TEXT("GamePlatformUI")));
    TestFalse(TEXT("活动已绑定页不可改固定身份"), Model->ConfigurePluginContext(TEXT("GamePlatformUI")));
    Model->Snapshot.Features.AddDefaulted_GetRef().Feature.FeatureId = TEXT("UI.Foreign");
    Model->Snapshot.Features.Last().Feature.PluginId = TEXT("GamePlatformUI");
    TestFalse(TEXT("旧值项不能跨PluginId选择功能"), Model->SelectFeature(TEXT("UI.Foreign")));
    Model->Snapshot.Scenarios.AddDefaulted_GetRef().Scenario.ScenarioId = TEXT("UI.Foreign.Normal");
    Model->Snapshot.Scenarios.Last().Scenario.FeatureId = TEXT("UI.Foreign");
    TestFalse(TEXT("关联案例不能越过固定插件执行归属"), Model->SelectScenario(TEXT("UI.Foreign.Normal")));
    const auto OldNotification = Catalog->OnCatalogChanged;
    Model->EndPage();
    const int32 InactiveRevision = Model->GetRevision();
    OldNotification.Broadcast();
    TestEqual(TEXT("失活后已投递旧目录回调无效"), Model->GetRevision(), InactiveRevision);
    TestTrue(TEXT("失活清除旧参数投影"), Model->GetMenuSnapshot().Features.IsEmpty());
    TestFalse(TEXT("失活页不能选择固定插件"), Model->SelectPlugin(TEXT("GamePlatformCore")));
    TestFalse(TEXT("失活页不能执行"), Model->RequestOperation(TEXT("Execute"), {}));
    Model->BeginPage();
    const int32 ReboundRevision = Model->GetRevision();
    OldNotification.Broadcast();
    TestEqual(TEXT("上一页代次不能刷新重开页面"), Model->GetRevision(), ReboundRevision);
    Catalog->OnCatalogChanged.Broadcast();
    TestEqual(TEXT("重新激活只订阅一次"), Model->GetRevision(), ReboundRevision + 1);
    Model->BindCatalog(Catalog.Get());
    const int32 RebindRevision = Model->GetRevision();
    Catalog->OnCatalogChanged.Broadcast();
    TestEqual(TEXT("同目录重绑不会重复订阅"), Model->GetRevision(), RebindRevision + 1);

    TStrongObjectPtr<UDivineBeastsDemoHubScreen> Page(NewObject<UDivineBeastsDemoHubScreen>());
    Page->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"), Model.Get(), NAME_None,
        EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    Page->DemoModel = Model.Get();
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Replacement(NewObject<UDivineBeastsDemoHubViewModel>());
    Page->InitializeActivatableViewModel(Replacement.Get());
    Page->OnViewModelStateChanged(0, Model->GetPageGeneration());
    TestFalse(TEXT("换VM后旧模型不得恢复命令源"), Page->DemoModel == Model.Get());

    TStrongObjectPtr<UWorld> World(NewObject<UWorld>());
    World->SetGameInstance(Instance.Get());
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> Context(NewObject<UDivineBeastsDemoWorldSubsystem>(World.Get()));
    Context->SourceWorld = World.Get(); Context->OverlayGeneration = 4;
    Context->LastFeedback = FText::FromString(TEXT("当前页状态"));
    Context->OnScenarioOverlayReady(3, {}, FGamePlatformResult::Failure(TEXT("Old.Load"), TEXT("迟到失败")));
    TestEqual(TEXT("旧资源回调不能覆盖新页结果"), Context->GetLastFeedback().ToString(), FString(TEXT("当前页状态")));
    Context->PendingScreenRequestId = FGuid::NewGuid();
    Context->OnDemoScreenOpened(FGuid::NewGuid(), TEXT("DivineBeasts.Demo.Hub"), Page.Get());
    TestFalse(TEXT("旧请求GUID不能接管新活动页"), Context->ActiveScreen.IsValid());
    TestFalse(TEXT("页面清理中拒绝重入释放"), (Context->bReleasingDemoOverlay = true, Context->ReleaseDemoOverlay()));
    Context->bReleasingDemoOverlay = false;
    Catalog->PendingPluginPageId = TEXT("GamePlatformUI");
    Catalog->bPendingPluginPage = true;
    FDivineBeastsDemoRunSnapshot Failure;
    Failure.CleanupResult = FGamePlatformResult::Failure(TEXT("Demo.TestCleanupFailed"), TEXT("保留清理失败"));
    Catalog->OnScenarioStopped(*World, Failure);
    TestFalse(TEXT("清理失败撤销切页请求"), Catalog->bPendingPluginPage);
    TestTrue(TEXT("清理失败保留可读诊断"), Catalog->GetLastOperationMessage().ToString().Contains(TEXT("清理失败")));

    const FString Hub = FDivineBeastsPluginDemoMode::GetHubMapPackage();
    TestTrue(TEXT("合法开发Hub与显式资格可请求插件直达"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("插件直达不能单独取得免登录资格"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, false, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("镜头审核地图不能取得独立插件直达资格"),
        FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false,
            FDivineBeastsPluginDemoMode::GetCameraReviewMapPackage(), TEXT("GamePlatformCore")));
    for (const TCHAR* Invalid : {TEXT(""), TEXT("None"), TEXT("/Game/Core"), TEXT("Core UI"), TEXT("中文")})
        TestFalse(TEXT("非法直接PluginId拒绝"),
            FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, false, Hub, Invalid));
    TestFalse(TEXT("发布构建不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(false, true, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("专服不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, false, true, false, Hub, TEXT("GamePlatformCore")));
    TestFalse(TEXT("冲突模式不能直达"), FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(true, true, true, true, Hub, TEXT("GamePlatformCore")));
    // 缺资源/错CDO/UIManager失败必须撤候选并保留当前页，而不是只忽略旧回调。
    Context->ActiveScreen = Page.Get();
    Context->PagePluginId = NAME_None;
    Context->bPendingPage = true;
    Context->PendingPagePluginId = TEXT("GamePlatformCore");
    const uint64 MissingGeneration = Context->OverlayGeneration;
    Context->OnScenarioOverlayReady(MissingGeneration, {}, FGamePlatformResult::Failure(TEXT("Data.AssetMissing"), TEXT("目标Widget资源缺失")));
    TestTrue(TEXT("目标加载失败仍保留原页面实例"), Context->ActiveScreen.Get() == Page.Get());
    TestTrue(TEXT("目标加载失败没有提交另一插件上下文"), Context->PagePluginId.IsNone());
    TestFalse(TEXT("失败候选不再占用下一次显式重试"), Context->bPendingPage);
    TestFalse(TEXT("失败释放无效候选自身租约，不改原页租约"), Context->PendingUILease.IsValid());
    TestTrue(TEXT("资源失败保留具体原因"), Context->GetLastFeedback().ToString().Contains(TEXT("Widget资源缺失")));
    const auto WrongDefaults = Context->ValidatePageWidgetClass(UDivineBeastsDemoHubScreen::StaticClass(), TEXT("GamePlatformCore"));
    TestFalse(TEXT("Hub原生默认值不能冒充Core固定页"), WrongDefaults.IsSuccess());
    Context->bPendingPage = true;
    Context->PendingPagePluginId = TEXT("GamePlatformCore");
    Context->FailPendingPage(Context->OverlayGeneration, WrongDefaults);
    TestTrue(TEXT("错CDO保留同一个原页面，可重试或返回"), Context->ActiveScreen.Get() == Page.Get());
    TestTrue(TEXT("错CDO失败具体说明可见恢复入口"), Context->GetLastFeedback().ToString().Contains(TEXT("重试或返回中心")));
    Context->bPendingPage = true;
    Context->PendingPageScreenId = TEXT("DivineBeasts.Demo.Pending.Test");
    Context->PendingScreenRequestId = FGuid::NewGuid();
    Context->OnDemoScreenFailed(Context->PendingScreenRequestId, Context->PendingPageScreenId, FText::FromString(TEXT("平台打开资源失败")));
    TestTrue(TEXT("UIManager失败不撤原页面"), Context->ActiveScreen.Get() == Page.Get());
    TestFalse(TEXT("UIManager失败撤销候选请求"), Context->PendingScreenRequestId.IsValid());
    TestTrue(TEXT("UIManager失败保留具体原因"), Context->GetLastFeedback().ToString().Contains(TEXT("平台打开资源失败")));
    Catalog->PublishPageNavigationResult(FGamePlatformResult::Failure(TEXT("Demo.PageOpenFailed"), TEXT("目标页失败；可重试或返回")));
    TestEqual(TEXT("失败事件投影到当前VM而非只写LastFeedback"), Model->GetMenuSnapshot().PageNavigationResult.Message,
        FString(TEXT("目标页失败；可重试或返回")));

    // 初次直达的真实触发状态：无ActiveScreen，但中心候选和其有效租约仍在准备。
    FDemoPageLeaseFaultService DeferredData;
    Context->CancelPendingPage(&DeferredData);
    Context->ActiveScreen.Reset(); Context->UILease = {}; Context->bRegisteredDemoScreen = false;
    const auto HubLease = DeferredData.Issue(TEXT("/Test/T02/Hub.Hub"));
    Context->PendingUILease = HubLease; Context->bPendingPage = true; Context->PendingPagePluginId = NAME_None;
    Context->PendingPageScreenId = TEXT("DivineBeasts.Demo.Hub");
    const uint64 HubGeneration = Context->OverlayGeneration;
    TestTrue(TEXT("中心候选租约有效且有签发记录"), HubLease.IsValid() && DeferredData.Issued.Contains(HubLease.LeaseId));
    TestTrue(TEXT("无活动中心的直达只排队"), Context->PrepareQualifiedPluginPage(*World, TEXT("GamePlatformCore"),
        FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_GamePlatformCore.WBP_DBA_PluginTest_GamePlatformCore_C"))));
    TestFalse(TEXT("排队时没有伪造活动中心"), Context->ActiveScreen.IsValid());
    TestTrue(TEXT("直达没有取消正在加载的中心"), Context->bPendingPage && Context->PendingPagePluginId.IsNone());
    TestEqual(TEXT("中心原租约身份不被目标替换"), Context->PendingUILease.LeaseId, HubLease.LeaseId);
    TestEqual(TEXT("中心候选代次未被直达取消"), Context->OverlayGeneration, HubGeneration);
    TestTrue(TEXT("直达意图绑定当前中心代次"), Context->bDeferredPluginPage && Context->DeferredHubGeneration == HubGeneration);
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> HubView(NewObject<UDivineBeastsDemoHubViewModel>());
    HubView->BindCatalog(Catalog.Get()); HubView->BeginPage();
    TStrongObjectPtr<UDivineBeastsDemoHubScreen> OpenedHub(NewObject<UDivineBeastsDemoHubScreen>());
    OpenedHub->InitializeScreen(TEXT("DivineBeasts.Demo.Hub"), HubView.Get(), NAME_None,
        EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    Context->bOpeningScreen = true; Context->OpeningScreenModel = HubView.Get();
    Context->OnDemoScreenOpened(FGuid::NewGuid(), TEXT("DivineBeasts.Demo.Hub"), OpenedHub.Get());
    Context->bOpeningScreen = false;
    TestTrue(TEXT("活动中心只能由真实Opened处理提交"), Context->ActiveScreen.Get() == OpenedHub.Get());
    // 此夹具没有生产Data实例，目标准备明确失败；中心在失败之前已由Opened提交，非预置ActiveScreen。
    TestFalse(TEXT("目标Data不可用保持明确失败"), Context->PrepareQualifiedPluginPage(*World, TEXT("GamePlatformCore"),
        FSoftObjectPath(TEXT("/Test/T02/Missing.Missing_C"))));
    TestTrue(TEXT("初次直达目标失败后已有可恢复的中心"), Context->ActiveScreen.Get() == OpenedHub.Get());
    TestTrue(TEXT("初次直达目标失败保留具体原因"), Context->GetLastFeedback().ToString().Contains(TEXT("中央Data服务")));
    Catalog->PublishPageNavigationResult(FGamePlatformResult::Failure(TEXT("Demo.PageDataUnavailable"), Context->GetLastFeedback().ToString()));
    TestTrue(TEXT("Opened后的中心VM消费失败并保持活动"), HubView->IsPageActive() &&
        HubView->GetMenuSnapshot().PageNavigationResult.Message.Contains(TEXT("中央Data服务")));
    Context->ReleaseDemoOverlay(&DeferredData); HubView->EndPage();

    // World退出故障边界：三个均为该服务签发的有效租约，候选/旧页撤销失败不能跳过活动所有权。
    TStrongObjectPtr<UWorld> ExitWorld(UWorld::CreateWorld(EWorldType::Game, false));
    ON_SCOPE_EXIT { if (ExitWorld.IsValid()) ExitWorld->DestroyWorld(false); };
    if (!TestNotNull(TEXT("退出夹具真实World"), ExitWorld.Get())) return false;
    ExitWorld->SetGameInstance(Instance.Get());
    TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> ExitContext(NewObject<UDivineBeastsDemoWorldSubsystem>(ExitWorld.Get()));
    // ULocalPlayer的Within为Engine；本结构/清理夹具不初始化Manager或安装视口，不要求额外GI关联。
    TStrongObjectPtr<ULocalPlayer> LocalPlayer(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UGamePlatformUIManagerSubsystem> UIManager(NewObject<UGamePlatformUIManagerSubsystem>(LocalPlayer.Get()));
    TStrongObjectPtr<UGamePlatformUIScreenDefinition> Definition(NewObject<UGamePlatformUIScreenDefinition>());
    Definition->ScreenId = TEXT("T02.Exit.Active"); Definition->InputMode = EGamePlatformUIInputMode::GameOnly;
    Definition->WidgetClass = TSoftClassPtr<UGamePlatformUIScreen>(UDivineBeastsDemoHubScreen::StaticClass());
    TestTrue(TEXT("活动页定义真实注册"), UIManager->RegisterScreenDefinition(Definition.Get()));
    UIManager->OnScreenOpened.AddUniqueDynamic(ExitContext.Get(), &UDivineBeastsDemoWorldSubsystem::OnDemoScreenOpened);
    UIManager->OnScreenOpenFailed.AddUniqueDynamic(ExitContext.Get(), &UDivineBeastsDemoWorldSubsystem::OnDemoScreenFailed);
    ExitContext->DemoUIManager = UIManager.Get(); ExitContext->ActiveScreen = OpenedHub.Get();
    ExitContext->bRegisteredDemoScreen = true; ExitContext->RegisteredDemoScreenId = Definition->ScreenId;
    ExitContext->PageScreenDefinition = Definition.Get();
    APlayerController* Controller = ExitWorld->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("退出夹具真实输入控制器"), Controller)) return false;
    Controller->bShowMouseCursor = true; ExitContext->DemoInputController = Controller;
    ExitContext->bOriginalMouseCursorVisible = false;
    FDemoPageLeaseFaultService ExitData;
    ExitContext->PendingUILease = ExitData.Issue(TEXT("/Test/T02/Candidate.Candidate"));
    ExitContext->UILease = ExitData.Issue(TEXT("/Test/T02/Active.Active"));
    ExitContext->RetiredUILease = ExitData.Issue(TEXT("/Test/T02/Retired.Retired"));
    const auto CandidateId = ExitContext->PendingUILease.LeaseId;
    const auto RetiredId = ExitContext->RetiredUILease.LeaseId;
    TestTrue(TEXT("三个租约均有效，不以空句柄绕过失败分支"), ExitContext->PendingUILease.IsValid() &&
        ExitContext->UILease.IsValid() && ExitContext->RetiredUILease.IsValid() && ExitData.Issued.Num() == 3);
    ExitData.RejectRelease.Add(CandidateId); ExitData.RejectRelease.Add(RetiredId);
    ExitContext->bPendingPage = true;
    TestFalse(TEXT("有效候选撤销故障令退出报告失败"), ExitContext->ReleaseDemoOverlay(&ExitData));
    TestFalse(TEXT("候选撤销失败仍撤销活动页字段所有权"), ExitContext->ActiveScreen.IsValid());
    TestFalse(TEXT("候选撤销失败仍注销真实活动定义"), UIManager->HasScreenDefinition(Definition->ScreenId));
    TestFalse(TEXT("候选撤销失败仍解绑Opened事件"), UIManager->OnScreenOpened.IsAlreadyBound(ExitContext.Get(),
        &UDivineBeastsDemoWorldSubsystem::OnDemoScreenOpened));
    TestFalse(TEXT("候选撤销失败仍恢复输入所有权"), ExitContext->DemoInputController.IsValid());
    TestFalse(TEXT("真实控制器光标恢复原值"), Controller->bShowMouseCursor);
    TestFalse(TEXT("候选失败不跳过可成功释放的活动租约"), ExitContext->UILease.IsValid());
    TestEqual(TEXT("候选失败句柄完整保留供重试"), ExitContext->PendingUILease.LeaseId, CandidateId);
    TestEqual(TEXT("旧页失败句柄也保留供重试"), ExitContext->RetiredUILease.LeaseId, RetiredId);
    TestTrue(TEXT("退出聚合候选与旧页两个具体失败"), ExitContext->GetLastFeedback().ToString().Contains(TEXT("候选租约")) &&
        ExitContext->GetLastFeedback().ToString().Contains(TEXT("旧页租约")));
    ExitData.RejectRelease.Reset();
    TestTrue(TEXT("下一次显式退出重试可收敛剩余租约"), ExitContext->ReleaseDemoOverlay(&ExitData));
    TestFalse(TEXT("重试成功清空候选失败句柄"), ExitContext->PendingUILease.IsValid());
    TestFalse(TEXT("重试成功清空旧页失败句柄"), ExitContext->RetiredUILease.IsValid());

    // 启动参数可控夹具只读测试公开资格；离开作用域恢复原命令行，不启动/切换地图。
    struct FScopedDemoArguments
    {
        FString Previous = FCommandLine::Get();
        ~FScopedDemoArguments() { FCommandLine::Set(*Previous); }
    } Arguments;
    const FString CameraArgs = FString(TEXT("-DBAPluginDemo -DBAPluginDemoMap=")) +
        FDivineBeastsPluginDemoMode::GetCameraReviewMapPackage() + TEXT(" -DBAPluginTest=GamePlatformCore");
    FCommandLine::Set(*CameraArgs);
    TestTrue(TEXT("镜头专项原资格仍保留"), FDivineBeastsPluginDemoMode::IsEnabled());
    TestFalse(TEXT("镜头启动下中心共用资格拒绝"), FDivineBeastsPluginDemoMode::IsHubPageEnabled());
    TestTrue(TEXT("镜头启动下直达同样拒绝"), FDivineBeastsPluginDemoMode::GetRequestedPluginTest().IsNone());
    TStrongObjectPtr<UWorld> HubWorld(NewObject<UWorld>(CreatePackage(*Hub), TEXT("T02_QualificationHub"), RF_Transient));
    HubWorld->WorldType = EWorldType::Game; HubWorld->SetGameInstance(Instance.Get());
    TestFalse(TEXT("镜头启动不能通过中心World资格"), Catalog->CanOpenPluginPageInWorld(*HubWorld));
    TestFalse(TEXT("镜头启动公开中心命令拒绝"), Catalog->OpenPluginPage(TEXT("GamePlatformCore")));
    const FString HubArgs = FString(TEXT("-DBAPluginDemo -DBAPluginDemoMap=")) + Hub + TEXT(" -DBAPluginTest=GamePlatformCore");
    FCommandLine::Set(*HubArgs);
    TestTrue(TEXT("Hub启动下中心共用资格通过"), FDivineBeastsPluginDemoMode::IsHubPageEnabled());
    TestEqual(TEXT("Hub启动下直达使用同一资格"), FDivineBeastsPluginDemoMode::GetRequestedPluginTest(), FName(TEXT("GamePlatformCore")));
    TestTrue(TEXT("Hub启动当前HubWorld合法"), Catalog->CanOpenPluginPageInWorld(*HubWorld));
    TStrongObjectPtr<UWorld> CaseWorld(NewObject<UWorld>(CreatePackage(TEXT("/Game/Development/PluginDemos/Maps/T02_Registered")), TEXT("T02_QualificationCase"), RF_Transient));
    CaseWorld->WorldType = EWorldType::Game; CaseWorld->SetGameInstance(Instance.Get());
    TestFalse(TEXT("Hub启动不开放任意World"), Catalog->CanOpenPluginPageInWorld(*CaseWorld));
    Catalog->Scenarios.AddDefaulted_GetRef().MapAsset = FSoftObjectPath(TEXT("/Game/Development/PluginDemos/Maps/T02_Registered.T02_Registered"));
    TestTrue(TEXT("Hub启动后登记案例World仍可导航"), Catalog->CanOpenPluginPageInWorld(*CaseWorld));
    Model->EndPage();
    return true;
}
#endif
