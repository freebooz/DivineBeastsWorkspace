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

namespace
{
/** 三项真实派生资产须经Monolith/UE制作；缺失只显示阻塞，不用旧界面假装新案例菜单。 */
const FSoftObjectPath DemoRoot(TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoRoot.WBP_DBA_PluginDemoRoot_C"));
const FSoftObjectPath DemoScreenDefinition(TEXT("/DBAUIPack_Core/Development/PluginDemos/Definitions/DA_DBA_PluginDemoHubScreen.DA_DBA_PluginDemoHubScreen"));
const FName DemoScreenId(TEXT("DivineBeasts.Demo.Hub"));
}
void UDivineBeastsDemoWorldSubsystem::RequestScenarioOverlay(UWorld& World)
{
    if (bOverlayClosing || OverlayGeneration == MAX_uint64) return;
    ReleaseDemoOverlay(); const uint64 Generation = OverlayGeneration;
    auto* Instance = World.GetGameInstance(); auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data) { LastFeedback = FText::FromString(TEXT("新案例页面缺中央Data服务。")); return; }
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this); FGamePlatformResult Accepted;
    UILease = Data->AcquireResources({DemoRoot, DemoScreenDefinition}, EGamePlatformDataLifetime::World, this,
        [WeakThis, Generation](const auto& Lease, const auto& Result)
        { if (auto* Self = WeakThis.Get()) Self->OnScenarioOverlayReady(Generation, Lease, Result); }, Accepted);
    if (!Accepted.IsSuccess()) LastFeedback = FText::FromString(TEXT("新页面资源请求拒绝：") + Accepted.Message);
}
void UDivineBeastsDemoWorldSubsystem::OnScenarioOverlayReady(uint64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    auto* World = GetWorld();
    if (bOverlayClosing || Generation != OverlayGeneration || !World || World->bIsTearingDown || World != SourceWorld.Get()) return;
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
    auto* Model = NewObject<UDivineBeastsDemoHubViewModel>(this);
    Model->BindCatalog(World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>());
    bOpeningScreen = true; bSynchronousScreenFailure = false;
    const auto Request = Manager->OpenScreenAsync(DemoScreenId, Model);
    bOpeningScreen = false;
    if (!bSynchronousScreenFailure && !bOverlayClosing && Generation == OverlayGeneration) PendingScreenRequestId = Request.RequestId;
    else if (Request.RequestId.IsValid()) Manager->CancelOpen(Request.RequestId);
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenOpened(FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen)
{
    if (bOverlayClosing || ScreenId != DemoScreenId || RequestId != PendingScreenRequestId) return;
    PendingScreenRequestId.Invalidate();
    if (!Cast<UDivineBeastsDemoHubScreen>(Screen))
    { LastFeedback = FText::FromString(TEXT("Monolith新页面父类不匹配。")); if (auto* Manager = DemoUIManager.Get()) Manager->CloseScreen(Screen); return; }
    ActiveScreen = Screen; LastFeedback = FText::FromString(TEXT("开发目录页面已入平台层栈，未进行人工核验。"));
    if (auto* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>()) Catalog->StartPendingScenario(*GetWorld());
}
void UDivineBeastsDemoWorldSubsystem::OnDemoScreenFailed(FGuid RequestId, FName ScreenId, FText Reason)
{
    if (bOverlayClosing || ScreenId != DemoScreenId || (!bOpeningScreen && RequestId != PendingScreenRequestId)) return;
    if (bOpeningScreen) bSynchronousScreenFailure = true;
    PendingScreenRequestId.Invalidate(); LastFeedback = Reason;
}
