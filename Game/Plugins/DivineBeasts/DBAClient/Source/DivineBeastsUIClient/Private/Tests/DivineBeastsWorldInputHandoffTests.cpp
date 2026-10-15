// 项目本地玩家UI输入交接回归：真实CommonUI Router保存/发布配置，验证旧UIOnly不能永久阻挡世界输入。
// 使用无视口LocalPlayer和真实Router对象，不激活页面、连接后端或授予Gameplay；不伪造Router成功结果。
// 测试调用项目真实关闭事件和Router真实配置事件，所有临时UObject由强引用保活，结束撤销精确委托。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#include "CommonInputModeTypes.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "DivineBeastsUIClientSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldInputHandoffTest,
    "DivineBeasts.UI.Input.WorldHandoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldInputHandoffTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("输入交接夹具需要真实Engine"), GEngine)) return false;
    TStrongObjectPtr<ULocalPlayer> TestPlayer(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UCommonUIActionRouterBase> Router(NewObject<UCommonUIActionRouterBase>(TestPlayer.Get()));
    TStrongObjectPtr<UDivineBeastsUIClientSubsystem> UI(NewObject<UDivineBeastsUIClientSubsystem>(TestPlayer.Get()));
    // 无视口夹具每次真实配置变化都会产生此原生警告；严格核对七次，不屏蔽其他错误或警告。
    // 此测试验证Router状态/事件，不宣称无视口也已应用Slate焦点、鼠标或控制器Ignore计数。
    AddExpectedMessagePlain(TEXT("Failed to commit change! GameViewportClient was null."), ELogVerbosity::Warning,
        EAutomationExpectedMessageFlags::Contains, 7);
    UI->ObserveWorldInputRouter(Router.Get());
    TestTrue(TEXT("项目Owner订阅真实Router配置变化"), Router->OnActiveInputConfigChanged().IsBoundToObject(UI.Get()));
    TestTrue(TEXT("项目Owner订阅真实Router树交接"), Router->OnBoundActionsUpdated().IsBoundToObject(UI.Get()));
    const auto UIOnly = UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::UIOnly);
    Router->SetActiveUIInputConfig(UIOnly);
    TestEqual(TEXT("前端保留真实Menu配置，不能强制改世界基线"), Router->GetActiveInputMode(), ECommonInputMode::Menu);

    // 复现加载页逻辑失活/关闭，而Slate尚未释放时Router仍保留UIOnly的真实状态。
    UI->ViewState.bAuthenticated = true;
    UI->ViewState.CurrentStep = TEXT("DBA.Flow.InWorld");
    UI->ViewState.PageState = EDivineBeastsUIPageState::Ready;
    UI->ActivePrimaryScreenId = TEXT("UI.Screen.LoadingTravel");
    UI->HandlePrimaryScreenClosed(TEXT("UI.Screen.LoadingTravel"));
    TestEqual(TEXT("世界页关闭交接同步Router为GameAndUI"), Router->GetActiveInputMode(), ECommonInputMode::All);
    TestEqual(TEXT("世界基线仅按下时临时捕获，允许首次鼠标按下进入游戏"), Router->GetActiveMouseCaptureMode(), EMouseCaptureMode::CaptureDuringMouseDown);

    // 在途菜单尚无Widget也必须拥有输入优先权，不能在Router先发布UIOnly的窗口被世界反抢。
    UI->OpeningSystemScreenId = TEXT("UI.Screen.SystemMenu");
    Router->SetActiveUIInputConfig(UIOnly);
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("正在打开系统菜单保留Menu"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->OpeningSystemScreenId = NAME_None;
    UI->bSystemMenuVisible = true;
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("活动系统菜单保留Menu"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->bSystemMenuVisible = false;

    UI->OpeningPrimaryScreenId = TEXT("UI.Screen.LoadingTravel");
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("正在打开主流程页保留其输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->OpeningPrimaryScreenId = NAME_None;
    UI->OpeningPrimaryRequestId = FGuid::NewGuid();
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("主页面请求身份仍在途时保留其输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->OpeningPrimaryRequestId.Invalidate();
    UI->OpeningSystemRequestId = FGuid::NewGuid();
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("系统页面请求身份仍在途时保留其输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->OpeningSystemRequestId.Invalidate();
    UI->ActivePrimaryScreenId = TEXT("UI.Screen.LoadingTravel");
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("主页面尚未真正关闭时保留其输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->ActivePrimaryScreenId = NAME_None;
    UI->ViewState.Loading.bIsLoading = true;
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("旅行/加载状态拒绝世界接管"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->ViewState.Loading.bIsLoading = false;
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("真正无页面的世界树交接恢复All"), Router->GetActiveInputMode(), ECommonInputMode::All);

    // 模拟迟到Slate交接重新留下Menu：真实SetActiveUIInputConfig事件应立即校正，不靠Tick。
    Router->SetActiveUIInputConfig(UIOnly);
    TestEqual(TEXT("迟到Menu配置由事件恢复世界基线"), Router->GetActiveInputMode(), ECommonInputMode::All);
    UI->ViewState.CurrentStep = TEXT("DBA.Flow.Authentication");
    Router->SetActiveUIInputConfig(UIOnly);
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("离开世界后前端Menu不被恢复逻辑抢占"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->ViewState.CurrentStep = TEXT("DBA.Flow.InWorld");
    UI->bUIShuttingDown = true;
    Router->OnBoundActionsUpdated().Broadcast();
    TestEqual(TEXT("退出期间同步关闭事件不能重新发布世界输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->ObserveWorldInputRouter(nullptr);
    TestFalse(TEXT("退出解除Router配置委托"), Router->OnActiveInputConfigChanged().IsBoundToObject(UI.Get()));
    TestFalse(TEXT("退出解除Router树委托"), Router->OnBoundActionsUpdated().IsBoundToObject(UI.Get()));
    UI->bUIShuttingDown = false;
    TStrongObjectPtr<ULocalPlayer> OtherPlayer(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UCommonUIActionRouterBase> OtherRouter(NewObject<UCommonUIActionRouterBase>(OtherPlayer.Get()));
    UI->ObserveWorldInputRouter(OtherRouter.Get());
    TestFalse(TEXT("拒绝订阅其他LocalPlayer的Router"), OtherRouter->OnBoundActionsUpdated().IsBoundToObject(UI.Get()));
    return true;
}
#endif
