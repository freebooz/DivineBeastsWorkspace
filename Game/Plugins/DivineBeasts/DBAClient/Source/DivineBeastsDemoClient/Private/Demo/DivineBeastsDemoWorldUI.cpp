// SSLM开发页面装配；只消费平台LocalPlayer UIManager和Data，不制作控件树/替换正式业务路由。
// 本世界拥有定义租约及页面请求，Root由本地玩家平台服务持有，跨开发地图保留；关停只撤销本次注册。
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "UI/DivineBeastsDemoHubScreen.h"
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
bool UDivineBeastsDemoWorldSubsystem::RequestPluginPage(FName Id)
{
    auto* World = GetWorld();
    if (!World || World != SourceWorld.Get() || World->bIsTearingDown || bOverlayClosing ||
        !FDivineBeastsPluginDemoMode::IsEnabled() || HasActiveScenario() || bReleasingDemoOverlay) return false;
    const auto Cleanup = GetRunSnapshot().CleanupResult;
    if (Cleanup.Status != EGamePlatformResultStatus::NotExecuted && !Cleanup.IsSuccess())
    { LastFeedback = FText::FromString(TEXT("当前运行清理失败，保留当前页诊断。")); return false; }
    auto* Instance = World->GetGameInstance();
    auto* Catalog = Instance ? Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr;
    FSoftObjectPath Widget;
    if (!Id.IsNone() && (!Catalog || !Catalog->FindPluginPageAsset(Id, Widget)))
    { LastFeedback = FText::FromString(TEXT("目录没有该插件的合法独立页，禁止猜测或跨插件回退。")); return false; }
    PagePluginId = Id; PageWidgetAsset = Widget; bPageRouteInitialized = true;
    return RequestScenarioOverlay(*World);
}
bool UDivineBeastsDemoWorldSubsystem::RequestScenarioOverlay(UWorld& World)
{
    if (bOverlayClosing || OverlayGeneration == MAX_uint64) return false;
    if (!ReleaseDemoOverlay()) return false;
    const uint64 Generation = OverlayGeneration;
    auto* Instance = World.GetGameInstance(); auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data) { LastFeedback = FText::FromString(TEXT("新案例页面缺中央Data服务。")); return false; }
    // 新世界沿用目录持有的页面身份；运行仍由本World唯一状态机接管，UI不重放Prepare。
    if (!bPageRouteInitialized)
    {
        bPageRouteInitialized = true;
        auto* Catalog = Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();
        if (Catalog && !Catalog->GetPluginPageContext().IsNone())
        {
            FSoftObjectPath Widget;
            if (!Catalog->FindPluginPageAsset(Catalog->GetPluginPageContext(), Widget))
            { LastFeedback = FText::FromString(TEXT("旅行后固定插件页未就绪，不回退到其他插件。")); return false; }
            PagePluginId = Catalog->GetPluginPageContext(); PageWidgetAsset = Widget;
        }
    }
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this); FGamePlatformResult Accepted;
    TArray<FSoftObjectPath> Resources{DemoRoot, DemoScreenDefinition};
    if (!PageWidgetAsset.IsNull()) Resources.Add(PageWidgetAsset);
    UILease = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const auto& Lease, const auto& Result)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioOverlayReady(Generation, Lease, Result); }, Accepted);
    if (!Accepted.IsSuccess()) LastFeedback = FText::FromString(TEXT("新页面资源请求拒绝：") + Accepted.Message);
    return Accepted.IsSuccess() && UILease.IsValid();
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioOverlayReady(uint64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* World = GetWorld();
    if (bOverlayClosing || Generation != OverlayGeneration || !World || World->bIsTearingDown || World != SourceWorld.Get())
    {
        // ReleaseDemoOverlay已撤销原租约；旧回调不得释放新代次或重新接管页面。
        return;
    }
    if (!Result.IsSuccess() || !Lease.IsValid() || Lease.ScopeId != UILease.ScopeId || Lease.LeaseId != UILease.LeaseId ||
        Lease.Generation != UILease.Generation || Lease.IssuerProof != UILease.IssuerProof || Lease.DefinitionId != UILease.DefinitionId ||
        Lease.Bundles != UILease.Bundles || Lease.ResourcePaths != UILease.ResourcePaths)
    { LastFeedback = FText::FromString(TEXT("Monolith开发页面资源缺失/租约不匹配：") + Result.Message); return; }
    auto* Root = Cast<UClass>(DemoRoot.ResolveObject());
    auto* Definition = Cast<UGamePlatformUIScreenDefinition>(DemoScreenDefinition.ResolveObject());
    auto* Player = World->GetGameInstance()->GetFirstGamePlayer();
    auto* Manager = Player ? Player->GetSubsystem<UGamePlatformUIManagerSubsystem>() : nullptr;
    FText Error;
    if (!Root || !Root->IsChildOf(UGamePlatformUILayerStack::StaticClass()) || Root->HasAnyClassFlags(CLASS_Abstract) || !Manager ||
        !Definition || Definition->ScreenId != DemoScreenId || !Definition->ValidateDefinition(Error))
    { LastFeedback = FText::FromString(TEXT("新RootLayout/ScreenDefinition/本地玩家不完整：") + Error.ToString()); return; }
    // 蓝图固定PluginId与Catalog软类必须一致；不能让一个合法类显示/执行另一个插件。
    UClass* WidgetClass = PageWidgetAsset.IsNull() ? Definition->WidgetClass.Get() : Cast<UClass>(PageWidgetAsset.ResolveObject());
    if (!WidgetClass && PageWidgetAsset.IsNull()) WidgetClass = Cast<UClass>(Definition->WidgetClass.ToSoftObjectPath().ResolveObject());
    const auto* Defaults = WidgetClass && WidgetClass->IsChildOf(UDivineBeastsDemoHubScreen::StaticClass()) &&
        !WidgetClass->HasAnyClassFlags(CLASS_Abstract) ? Cast<UDivineBeastsDemoHubScreen>(WidgetClass->GetDefaultObject()) : nullptr;
    if ((!PagePluginId.IsNone() && (!Defaults || Defaults->GetFixedPluginId() != PagePluginId)) ||
        (Defaults && Defaults->GetFixedPluginId() != PagePluginId))
    { LastFeedback = FText::FromString(TEXT("独立页父类或固定插件身份与目录不一致。")); return; }
    if (!PagePluginId.IsNone())
    {
        PageScreenDefinition = DuplicateObject<UGamePlatformUIScreenDefinition>(Definition, this);
        PageScreenDefinition->WidgetClass = TSoftClassPtr<UGamePlatformUIScreen>(PageWidgetAsset);
        // 禁止平台变体或默认回退把已核验的固定页换为另一插件。
        PageScreenDefinition->PlatformWidgetVariants.Reset();
        PageScreenDefinition->bSurvivesTravel = false;
        Definition = PageScreenDefinition;
        if (!Definition->ValidateDefinition(Error))
        { LastFeedback = Error; return; }
    }
    if (Manager->GetRootLayout() && Manager->GetRootLayout()->GetClass() != Root)
    { LastFeedback = FText::FromString(TEXT("本地玩家已有另一根布局，开发页不接管正式界面所有权。")); return; }
    if (!Manager->GetRootLayout() && !Manager->InstallRootLayoutClass(Root))
    { LastFeedback = FText::FromString(TEXT("平台拒绝安装开发根布局。")); return; }
    // 同步外部回调可能关停世界；返回后重验代次，不在旧栈继续注册。
    if (bOverlayClosing || Generation != OverlayGeneration || World->bIsTearingDown) return;
    if (Manager->HasScreenDefinition(DemoScreenId) || !Manager->RegisterScreenDefinition(Definition))
    { LastFeedback = FText::FromString(TEXT("页面身份已被其他注册占用或平台校验失败。")); return; }
    DemoUIManager = Manager; bRegisteredDemoScreen = true;
    Manager->OnScreenOpened.AddUniqueDynamic(this, &ThisClass::OnDemoScreenOpened);
    Manager->OnScreenOpenFailed.AddUniqueDynamic(this, &ThisClass::OnDemoScreenFailed);
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(NewObject<UDivineBeastsDemoHubViewModel>(this));
    if (!Model->ConfigurePluginContext(PagePluginId))
    { LastFeedback = FText::FromString(TEXT("固定插件上下文配置失败。")); return; }
    Model->BindCatalog(World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>());
    bOpeningScreen = true; bSynchronousScreenFailure = false; OpeningScreenModel=Model.Get();
    SynchronouslyOpenedScreenRequestId.Invalidate();
    const auto Request = Manager->OpenScreenAsync(DemoScreenId, Model.Get());
    if (bOverlayClosing || Generation != OverlayGeneration)
    { if (Request.RequestId.IsValid()) Manager->CancelOpen(Request.RequestId); return; }
    bOpeningScreen = false;
    OpeningScreenModel.Reset();
    if (!bSynchronousScreenFailure && !bOverlayClosing && Generation == OverlayGeneration)
    { if(SynchronouslyOpenedScreenRequestId!=Request.RequestId)PendingScreenRequestId = Request.RequestId; }
    else if (Request.RequestId.IsValid()) Manager->CancelOpen(Request.RequestId);
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenOpened(FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen)
{
    if (bOverlayClosing || ScreenId != DemoScreenId || !RequestId.IsValid()) return;
    if (bOpeningScreen)
    {
        // 不能将同步成功遗失，也不能接管同ID但属于其他模型的页面。
        if(!OpeningScreenModel.IsValid()||!Screen||Screen->GetViewModel()!=OpeningScreenModel.Get())return;
        SynchronouslyOpenedScreenRequestId=RequestId;
    }
    else if(RequestId!=PendingScreenRequestId)return;
    PendingScreenRequestId.Invalidate();
    const auto* Page = Cast<UDivineBeastsDemoHubScreen>(Screen);
    const auto* Model = Screen ? Cast<UDivineBeastsDemoHubViewModel>(Screen->GetViewModel()) : nullptr;
    if (!Page || !Model || Page->GetFixedPluginId() != PagePluginId || Model->GetFixedPluginId() != PagePluginId)
    { LastFeedback = FText::FromString(TEXT("Monolith新页面父类不匹配。")); if (auto* Manager = DemoUIManager.Get()) Manager->CloseScreen(Screen); return; }
    ActiveScreen = Screen; LastFeedback = FText::FromString(TEXT("开发目录页面已入平台层栈，未进行人工核验。"));
    if(auto* World=GetWorld())if(auto* Instance=World->GetGameInstance())
        if(auto* Catalog=Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>())Catalog->StartPendingScenario(*World);
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenFailed(FGuid RequestId, FName ScreenId, FText Reason)
{
    if (bOverlayClosing || ScreenId != DemoScreenId || (!bOpeningScreen && RequestId != PendingScreenRequestId)) return;
    if (bOpeningScreen) bSynchronousScreenFailure = true;
    PendingScreenRequestId.Invalidate(); LastFeedback = Reason;
}
