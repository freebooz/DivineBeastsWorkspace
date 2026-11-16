// SSLM开发页面装配；只消费平台LocalPlayer UIManager和Data，不制作控件树/替换正式业务路由。
// 本世界拥有定义租约及页面请求，Root由本地玩家平台服务持有，跨开发地图保留；关停只撤销本次注册。
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "UI/DivineBeastsDemoHubScreen.h"
#include "UI/DivineBeastsDemoHubWidget.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Development/DivineBeastsPluginDemoMode.h"

namespace
{
/** 三项真实派生资产须经Monolith/UE制作；缺失只显示阻塞，不用旧界面假装新案例菜单。 */
const FSoftObjectPath DemoRoot(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoRoot.WBP_DBA_PluginDemoRoot_C"));
const FSoftObjectPath DemoScreenDefinition(TEXT("/DBAUIPack_Core/Development/PluginDemos/Definitions/DA_DBA_PluginDemoHubScreen.DA_DBA_PluginDemoHubScreen"));
const FName DemoScreenId(TEXT("DivineBeasts.Demo.Hub"));
}
FGamePlatformResult UDivineBeastsDemoWorldSubsystem::ReleasePageLease(FGamePlatformDataLease& Lease)
{
    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    return ReleasePageLease(Lease, Instance ? IGamePlatformDataService::Get(*Instance) : nullptr);
}
FGamePlatformResult UDivineBeastsDemoWorldSubsystem::ReleasePageLease(FGamePlatformDataLease& Lease, IGamePlatformDataService* Data)
{
    if (!Lease.IsValid()) return FGamePlatformResult::Success();
    const auto Result = Data ? Data->ReleaseResources(Lease) :
        FGamePlatformResult::Failure(TEXT("Demo.PageDataUnavailable"), TEXT("中央Data不可用，页面租约未确认清理。"));
    if (Result.IsSuccess()) Lease = {};
    return Result;
}
void UDivineBeastsDemoWorldSubsystem::PublishPageResult(const FGamePlatformResult& Result)
{
    LastFeedback = FText::FromString(Result.Message);
    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (auto* Catalog = Instance ? Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr)
        Catalog->PublishPageNavigationResult(Result);
}
bool UDivineBeastsDemoWorldSubsystem::CancelPendingPage()
{
    auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    return CancelPendingPage(Instance ? IGamePlatformDataService::Get(*Instance) : nullptr);
}
bool UDivineBeastsDemoWorldSubsystem::CancelPendingPage(IGamePlatformDataService* Data)
{
    if (bReleasingDemoOverlay) return false;
    TGuardValue<bool> Releasing(bReleasingDemoOverlay, true);
    ++OverlayGeneration;
    // 先撤候选资格，再调用可能同步广播失败/关闭的UIManager；旧页账本完全不变。
    bPendingPage = false;
    bDeferredPluginPage = false; DeferredPluginPageId = NAME_None; DeferredHubGeneration = 0;
    const FGuid Request = PendingScreenRequestId;
    const FName ScreenId = PendingPageScreenId;
    const bool bRegistered = bPendingDefinitionRegistered;
    const TStrongObjectPtr<UGamePlatformUIScreenDefinition> Definition(PendingPageScreenDefinition);
    bool bDefinitionReleased = true;
    auto* Manager = PendingUIManager.Get();
    PendingScreenRequestId.Invalidate(); bPendingDefinitionRegistered = false;
    PendingScreenModel.Reset(); OpeningScreenModel.Reset();
    bOpeningScreen = false; bSynchronousScreenFailure = false;
    SynchronouslyOpenedScreenRequestId.Invalidate();
    PendingUIManager.Reset(); PendingPageScreenDefinition = nullptr;
    PendingPagePluginId = NAME_None; PendingPageWidgetAsset.Reset();
    if (Manager)
    {
        if (Request.IsValid()) Manager->CancelOpen(Request);
        if (bRegistered && Manager->HasScreenDefinition(ScreenId) && !Manager->UnregisterScreenDefinition(ScreenId))
        {
            bDefinitionReleased = false;
            PendingUIManager = Manager; bPendingDefinitionRegistered = true;
            PendingPageScreenDefinition = Definition.Get();
        }
        if (Manager != DemoUIManager.Get())
        {
            Manager->OnScreenOpened.RemoveDynamic(this, &ThisClass::OnDemoScreenOpened);
            Manager->OnScreenOpenFailed.RemoveDynamic(this, &ThisClass::OnDemoScreenFailed);
        }
    }
    const auto Released = ReleasePageLease(PendingUILease, Data);
    TArray<FString> Errors;
    if (!bDefinitionReleased) Errors.Add(TEXT("候选页面定义注销失败。"));
    if (!Released.IsSuccess()) Errors.Add(Released.Message);
    if (!Errors.IsEmpty()) LastFeedback = FText::FromString(FString::Join(Errors, TEXT("；")));
    return Errors.IsEmpty();
}
void UDivineBeastsDemoWorldSubsystem::FailPendingPage(uint64 ExpectedGeneration, const FGamePlatformResult& Result)
{
    if (ExpectedGeneration != OverlayGeneration || bOverlayClosing) return;
    const bool bReleased = CancelPendingPage();
    const auto Published = bReleased ? Result : FGamePlatformResult::Failure(TEXT("Demo.PagePreparationCleanupFailed"),
        Result.Message + TEXT("；候选资源清理仍失败：") + LastFeedback.ToString());
    // 当前页仍由平台持有且可操作；先完成撤销，再通过目录事件显示原始原因及重试/返回入口。
    PublishPageResult(Published);
}
FGamePlatformResult UDivineBeastsDemoWorldSubsystem::ValidatePageWidgetClass(UClass* WidgetClass, FName Id)
{
    const auto* Defaults = WidgetClass && WidgetClass->IsChildOf(UDivineBeastsDemoHubScreen::StaticClass()) &&
        !WidgetClass->HasAnyClassFlags(CLASS_Abstract) ? Cast<UDivineBeastsDemoHubScreen>(WidgetClass->GetDefaultObject()) : nullptr;
    return Defaults && Defaults->GetFixedPluginId() == Id ? FGamePlatformResult::Success() :
        FGamePlatformResult::Failure(TEXT("Demo.PluginPageIdentityMismatch"), TEXT("目标页父类或固定PluginId与目录不一致；保留当前页，可重试或返回中心。"));
}
bool UDivineBeastsDemoWorldSubsystem::RequestPluginPage(FName Id)
{
    auto* World = GetWorld();
    auto* Instance = World ? World->GetGameInstance() : nullptr;
    auto* Catalog = Instance ? Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr;
    if (!World || World != SourceWorld.Get() || bOverlayClosing || bReleasingDemoOverlay || HasActiveScenario()) return false;
    if (!Catalog || !Catalog->CanOpenPluginPageInWorld(*World))
    { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.HubPageQualification"), TEXT("独立插件页要求精确Hub启动资格及当前登记案例World。"))); return false; }
    const auto Cleanup = GetRunSnapshot().CleanupResult;
    if (Cleanup.Status != EGamePlatformResultStatus::NotExecuted && !Cleanup.IsSuccess())
    { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.PageRunCleanupFailed"), TEXT("当前运行清理失败，保留当前页诊断。"))); return false; }
    FSoftObjectPath Widget;
    if (!Id.IsNone() && !Catalog->FindPluginPageAsset(Id, Widget))
    { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.PluginPageNotRegistered"), TEXT("目录没有该插件的合法独立页；保留当前页，不猜测路径或跨插件回退。"))); return false; }
    return PrepareQualifiedPluginPage(*World, Id, Widget);
}
bool UDivineBeastsDemoWorldSubsystem::RequestScenarioOverlay(UWorld& World)
{
    auto* Instance = World.GetGameInstance();
    auto* Catalog = Instance ? Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr;
    if (!Catalog || !Catalog->CanOpenPluginPageInWorld(World)) return false;
    const FName Id = Catalog->GetPluginPageContext();
    FSoftObjectPath Widget;
    if (!Id.IsNone() && !Catalog->FindPluginPageAsset(Id, Widget))
    { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.PluginPageNotRegistered"), TEXT("旅行后固定插件页尚未就绪，不回退到其他插件。"))); return false; }
    return PrepareQualifiedPluginPage(World, Id, Widget);
}
bool UDivineBeastsDemoWorldSubsystem::PrepareQualifiedPluginPage(UWorld& World, FName Id, const FSoftObjectPath& Widget)
{
    if (bOverlayClosing || bReleasingDemoOverlay || World.bIsTearingDown || &World != SourceWorld.Get()) return false;
    const uint64 Intent = ++PageIntentGeneration;
    bDeferredPluginPage = false; DeferredPluginPageId = NAME_None; DeferredHubGeneration = 0;
    if (ActiveScreen.IsValid()) return BeginPagePreparation(World, Id, Widget);
    // 直达与旅行后的固定页一律等中心真实Opened。未完成的中心候选不能被它们取消。
    if (bPendingPage && PendingPagePluginId.IsNone() && PendingUILease.IsValid())
    {
        if (!Id.IsNone())
        { bDeferredPluginPage = true; DeferredPluginPageId = Id; DeferredHubGeneration = OverlayGeneration; }
        return true;
    }
    const bool Accepted = BeginPagePreparation(World, NAME_None, {});
    // 准备事件可同步触发更新的用户意图；旧调用返回后不能覆盖最新选择。
    if (Accepted && Intent == PageIntentGeneration && bPendingPage && PendingPagePluginId.IsNone() && !Id.IsNone())
    { bDeferredPluginPage = true; DeferredPluginPageId = Id; DeferredHubGeneration = OverlayGeneration; }
    return Accepted;
}
bool UDivineBeastsDemoWorldSubsystem::BeginPagePreparation(UWorld& World, FName Id, const FSoftObjectPath& Widget)
{
    if (bOverlayClosing || bReleasingDemoOverlay || OverlayGeneration == MAX_uint64 || &World != SourceWorld.Get()) return false;
    // 新意图只取消旧候选；旧活动页面、参数与UI租约继续由原所有者持有。
    if (!CancelPendingPage())
    { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.PagePreparationCleanupFailed"), LastFeedback.ToString())); return false; }
    const auto Retired = ReleasePageLease(RetiredUILease);
    if (!Retired.IsSuccess()) { PublishPageResult(Retired); return false; }
    const uint64 Generation = OverlayGeneration;
    auto* Instance = World.GetGameInstance();
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data) { PublishPageResult(FGamePlatformResult::Failure(TEXT("Demo.PageDataUnavailable"), TEXT("目标页缺中央Data服务；当前页保留，可重试或返回。"))); return false; }
    bPendingPage = true; PendingPagePluginId = Id; PendingPageWidgetAsset = Widget;
    PendingPageScreenId = !ActiveScreen.IsValid() && !bRegisteredDemoScreen ? DemoScreenId :
        FName(*(TEXT("DivineBeasts.Demo.Pending.") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    FGamePlatformResult Preparing;
    Preparing.Message = TEXT("正在准备目标页；当前页保留，页面打开与人工验收尚未完成。");
    PublishPageResult(Preparing);
    if (bOverlayClosing || Generation != OverlayGeneration || !bPendingPage) return false;
    TArray<FSoftObjectPath> Resources{DemoRoot, DemoScreenDefinition};
    if (!Widget.IsNull()) Resources.Add(Widget);
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    PendingUILease = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const auto& Lease, const auto& Result)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioOverlayReady(Generation, Lease, Result); }, Accepted);
    if (!Accepted.IsSuccess() || !PendingUILease.IsValid())
    {
        FailPendingPage(Generation, Accepted.IsSuccess() ? FGamePlatformResult::Failure(TEXT("Demo.PageLeaseInvalid"), TEXT("目标页请求未取得有效Data租约。")) : Accepted);
        return false;
    }
    return true;
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioOverlayReady(uint64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* World = GetWorld();
    if (bOverlayClosing || !bPendingPage || Generation != OverlayGeneration || !World || World->bIsTearingDown || World != SourceWorld.Get()) return;
    if (!Result.IsSuccess()) { FailPendingPage(Generation, Result); return; }
    if (HasActiveScenario())
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageRunBecameActive"), TEXT("准备期间原页已开始案例，保留原页，不提交新的插件页。"))); return; }
    if (!Lease.IsValid() || Lease.ScopeId != PendingUILease.ScopeId || Lease.LeaseId != PendingUILease.LeaseId ||
        Lease.Generation != PendingUILease.Generation || Lease.IssuerProof != PendingUILease.IssuerProof || Lease.DefinitionId != PendingUILease.DefinitionId ||
        Lease.Bundles != PendingUILease.Bundles || Lease.ResourcePaths != PendingUILease.ResourcePaths)
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageLeaseMismatch"), TEXT("目标页资源租约不匹配；当前页保留。"))); return; }
    auto* Root = Cast<UClass>(DemoRoot.ResolveObject());
    auto* Definition = Cast<UGamePlatformUIScreenDefinition>(DemoScreenDefinition.ResolveObject());
    auto* Player = World->GetGameInstance()->GetFirstGamePlayer();
    auto* Manager = Player ? Player->GetSubsystem<UGamePlatformUIManagerSubsystem>() : nullptr;
    FText Error;
    if (!Root || !Root->IsChildOf(UGamePlatformUILayerStack::StaticClass()) || Root->HasAnyClassFlags(CLASS_Abstract) || !Manager ||
        !Definition || Definition->ScreenId != DemoScreenId || !Definition->ValidateDefinition(Error))
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageContractMissing"), TEXT("目标RootLayout/ScreenDefinition/本地玩家不完整：") + Error.ToString())); return; }
    UClass* WidgetClass = PendingPageWidgetAsset.IsNull() ? Definition->WidgetClass.Get() : Cast<UClass>(PendingPageWidgetAsset.ResolveObject());
    if (!PendingPagePluginId.IsNone() || WidgetClass)
    {
        const auto Valid = ValidatePageWidgetClass(WidgetClass, PendingPagePluginId);
        if (!Valid.IsSuccess()) { FailPendingPage(Generation, Valid); return; }
    }
    PendingPageScreenDefinition = DuplicateObject<UGamePlatformUIScreenDefinition>(Definition, this);
    PendingPageScreenDefinition->ScreenId = PendingPageScreenId;
    PendingPageScreenDefinition->bSurvivesTravel = false;
    if (!PendingPagePluginId.IsNone())
    {
        PendingPageScreenDefinition->WidgetClass = TSoftClassPtr<UGamePlatformUIScreen>(PendingPageWidgetAsset);
        PendingPageScreenDefinition->PlatformWidgetVariants.Reset();
    }
    if (!PendingPageScreenDefinition->ValidateDefinition(Error))
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageDefinitionInvalid"), Error.ToString())); return; }
    if (Manager->GetRootLayout() && Manager->GetRootLayout()->GetClass() != Root)
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageRootOwnership"), TEXT("本地玩家已有另一根布局，开发页不接管正式界面。"))); return; }
    if (!Manager->GetRootLayout() && !Manager->InstallRootLayoutClass(Root))
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageRootRejected"), TEXT("平台拒绝安装开发根布局。"))); return; }
    if (bOverlayClosing || Generation != OverlayGeneration || World->bIsTearingDown) return;
    if (Manager->HasScreenDefinition(PendingPageScreenId) || !Manager->RegisterScreenDefinition(PendingPageScreenDefinition))
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageRegistrationRejected"), TEXT("候选页面身份被占用或平台校验失败；当前页保留。"))); return; }
    PendingUIManager = Manager; bPendingDefinitionRegistered = true;
    Manager->OnScreenOpened.AddUniqueDynamic(this, &ThisClass::OnDemoScreenOpened);
    Manager->OnScreenOpenFailed.AddUniqueDynamic(this, &ThisClass::OnDemoScreenFailed);
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>(this));
    if (!Model->ConfigurePluginContext(PendingPagePluginId))
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageModelContext"), TEXT("目标页固定插件上下文配置失败。"))); return; }
    Model->BindCatalog(World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>());
    bOpeningScreen = true; bSynchronousScreenFailure = false;
    OpeningScreenModel = Model.Get(); PendingScreenModel = Model.Get();
    SynchronouslyOpenedScreenRequestId.Invalidate();
    const auto Request = Manager->OpenScreenAsync(PendingPageScreenId, Model.Get());
    if (bOverlayClosing || Generation != OverlayGeneration)
    { if (Request.RequestId.IsValid()) Manager->CancelOpen(Request.RequestId); return; }
    bOpeningScreen = false; OpeningScreenModel.Reset();
    if (!bPendingPage) return; // 同步Opened已提交或Failed已撤销，不能覆盖其终态。
    if (!Request.RequestId.IsValid())
    { FailPendingPage(Generation, FGamePlatformResult::Failure(TEXT("Demo.PageOpenNotAccepted"), TEXT("平台未受理候选页打开；当前页保留。"))); return; }
    if (SynchronouslyOpenedScreenRequestId != Request.RequestId) PendingScreenRequestId = Request.RequestId;
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenOpened(FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen)
{
    if (bOverlayClosing || ScreenId != PendingPageScreenId || !RequestId.IsValid()) return;
    if (bOpeningScreen)
    {
        if (!OpeningScreenModel.IsValid() || !Screen || Screen->GetViewModel() != OpeningScreenModel.Get()) return;
        SynchronouslyOpenedScreenRequestId = RequestId;
    }
    else if (!bPendingPage || RequestId != PendingScreenRequestId || !PendingScreenModel.IsValid() ||
        !Screen || Screen->GetViewModel() != PendingScreenModel.Get()) return;
    const uint64 Generation = OverlayGeneration;
    const TStrongObjectPtr<UDivineBeastsDemoWorldSubsystem> KeepContext(this);
    const TStrongObjectPtr<UGamePlatformUIScreen> KeepScreen(Screen);
    const auto* Page = Cast<UDivineBeastsDemoHubScreen>(Screen);
    const auto* Model = Screen ? Cast<UDivineBeastsDemoHubViewModel>(Screen->GetViewModel()) : nullptr;
    const auto Valid = HasActiveScenario()
        ? FGamePlatformResult::Failure(TEXT("Demo.PageRunBecameActive"), TEXT("准备期间原页已开始案例，保留原页，不提交新的插件页。"))
        : (Page && Model && Page->GetFixedPluginId() == PendingPagePluginId && Model->GetFixedPluginId() == PendingPagePluginId
            ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(TEXT("Demo.PageOpenedIdentityMismatch"), TEXT("平台打开的目标页/模型固定身份不一致；保留原页。")));
    if (!Valid.IsSuccess())
    {
        if (auto* Manager = PendingUIManager.Get()) Manager->CloseScreen(Screen);
        FailPendingPage(Generation, Valid); return;
    }
    FGamePlatformResult Released;
    {
        TGuardValue<bool> Committing(bReleasingDemoOverlay, true);
        auto* PreviousManager = DemoUIManager.Get();
        auto* PreviousScreen = ActiveScreen.Get();
        const FName PreviousId = RegisteredDemoScreenId;
        const bool bPreviousRegistered = bRegisteredDemoScreen;
        RetiredUILease = UILease; UILease = PendingUILease; PendingUILease = {};
        DemoUIManager = PendingUIManager; ActiveScreen = Screen;
        RegisteredDemoScreenId = PendingPageScreenId; bRegisteredDemoScreen = bPendingDefinitionRegistered;
        PageScreenDefinition = PendingPageScreenDefinition; PendingPageScreenDefinition = nullptr;
        PagePluginId = PendingPagePluginId; PageWidgetAsset = PendingPageWidgetAsset; bPageRouteInitialized = true;
        bPendingPage = false; bPendingDefinitionRegistered = false;
        PendingScreenRequestId.Invalidate(); PendingScreenModel.Reset(); PendingUIManager.Reset();
        PendingPagePluginId = NAME_None; PendingPageWidgetAsset.Reset();
        if (PreviousManager)
        {
            if (PreviousScreen && PreviousScreen != Screen) PreviousManager->CloseScreen(PreviousScreen);
            if (bPreviousRegistered) PreviousManager->UnregisterScreenDefinition(PreviousId);
            if (PreviousManager != DemoUIManager.Get())
            {
                PreviousManager->OnScreenOpened.RemoveDynamic(this, &ThisClass::OnDemoScreenOpened);
                PreviousManager->OnScreenOpenFailed.RemoveDynamic(this, &ThisClass::OnDemoScreenFailed);
            }
        }
        if (auto* Widget = ActiveWidget.Get()) Widget->RemoveFromParent();
        ActiveWidget.Reset();
        Released = ReleasePageLease(RetiredUILease);
        if (!bOverlayClosing && Generation == OverlayGeneration && GetWorld() && !GetWorld()->bIsTearingDown)
            if (auto* Instance = GetWorld()->GetGameInstance())
                if (auto* Catalog = Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>()) Catalog->CommitPluginPageContext(PagePluginId);
    }
    if (bOverlayClosing || Generation != OverlayGeneration || !GetWorld() || GetWorld()->bIsTearingDown || ActiveScreen.Get() != Screen) return;
    if (!Released.IsSuccess()) { PublishPageResult(Released); return; }
    auto Opened = FGamePlatformResult::Success();
    Opened.Message = TEXT("目标开发页已由平台打开，原页已撤销；功能运行和人工验收分别确认。");
    PublishPageResult(Opened);
    if (bOverlayClosing || Generation != OverlayGeneration || ActiveScreen.Get() != Screen) return;
    if (PagePluginId.IsNone() && bDeferredPluginPage && DeferredHubGeneration == Generation)
    {
        const FName Requested = DeferredPluginPageId;
        bDeferredPluginPage = false; DeferredPluginPageId = NAME_None; DeferredHubGeneration = 0;
        // 只有中心实例已成为活动页才提交目标；目标失败后该中心仍消费具体错误并提供重试/返回。
        RequestPluginPage(Requested);
        return;
    }
    if (auto* World = GetWorld(); World && !World->bIsTearingDown)
        if (auto* Instance = World->GetGameInstance())
            if (auto* Catalog = Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>()) Catalog->StartPendingScenario(*World);
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenFailed(FGuid RequestId, FName ScreenId, FText Reason)
{
    if (bOverlayClosing || !bPendingPage || ScreenId != PendingPageScreenId ||
        (!bOpeningScreen && RequestId != PendingScreenRequestId)) return;
    if (bOpeningScreen) bSynchronousScreenFailure = true;
    FailPendingPage(OverlayGeneration, FGamePlatformResult::Failure(TEXT("Demo.PageOpenFailed"), Reason.ToString()));
}
