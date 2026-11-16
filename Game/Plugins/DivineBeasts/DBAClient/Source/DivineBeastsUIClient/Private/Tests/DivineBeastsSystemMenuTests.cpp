// 路由回归覆盖前端/传输/错误/注销门禁，以及设置关闭后的真实Router交接；不替代视口键鼠及联网验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonInputModeTypes.h"
#include "Input/GamePlatformUIInputPolicy.h"
#include "ViewModels/System/DivineBeastsSystemSettingsViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSystemMenuRoutingTest, "DivineBeasts.UI.SystemMenu.WorldRouting", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSystemMenuRoutingTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    TestFalse(TEXT("缺省启动不得打开世界菜单"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bAuthenticated = true;
    for (const TCHAR* Step : {TEXT("DBA.Flow.Authentication"), TEXT("DBA.Flow.CharacterEntry"), TEXT("DBA.Flow.TransferWorld"), TEXT("DBA.Flow.WorldReady")})
    {
        State.CurrentStep = Step;
        TestFalse(TEXT("前端与加载步骤保留原页取消，不开世界菜单"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    }
    State.CurrentStep = TEXT("DBA.Flow.InWorld");
    TestTrue(TEXT("已认证空闲InWorld可以打开"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bBusy = true;
    TestFalse(TEXT("忙碌拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.bBusy = false; State.Loading.bIsLoading = true;
    TestFalse(TEXT("旅行拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.Loading.bIsLoading = false; State.PageState = EDivineBeastsUIPageState::Error;
    TestFalse(TEXT("错误拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    State.PageState = EDivineBeastsUIPageState::Ready; State.bAuthenticated = false;
    TestFalse(TEXT("注销拒绝"), FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(State));
    for (const TCHAR* Id : {TEXT("UI.Screen.SystemMenu"), TEXT("UI.Screen.Settings")})
    {
        const auto* Entry = FDivineBeastsUIScreenCatalog::Find(Id);
        TestNotNull(TEXT("真实系统功能有稳定目录"), Entry);
        if (!Entry) continue;
        TestEqual(TEXT("只消费UI输入"), Entry->InputMode, EGamePlatformUIInputMode::UIOnly);
        TestEqual(TEXT("联网世界不Pause"), Entry->PausePolicy, EGamePlatformUIPausePolicy::Never);
        TestFalse(TEXT("旅行不保留捕获页面"), Entry->bSurvivesTravel);
        TestEqual(TEXT("系统层栈"), Entry->Layer, EGamePlatformUILayer::System);
    }
    return true;
}
/** 齿轮直达设置后，关闭必须释放系统输入占用；从菜单进入则保留父菜单，迟到通知不能撤销新请求。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsSkillBarSettingsCloseTest,
    "DivineBeasts.UI.SystemMenu.SkillBarSettingsClose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsSkillBarSettingsCloseTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("测试引擎"), GEngine)) return false;
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    TStrongObjectPtr<UDivineBeastsUIClientSubsystem> UI(NewObject<UDivineBeastsUIClientSubsystem>(Player.Get()));
    TStrongObjectPtr<UCommonUIActionRouterBase> Router(NewObject<UCommonUIActionRouterBase>(Player.Get()));
    // 无视口夹具只验证真实Router状态，不声称已覆盖物理键鼠；四次模式切换各产生一次原生提示。
    AddExpectedMessagePlain(TEXT("Failed to commit change! GameViewportClient was null."), ELogVerbosity::Warning,
        EAutomationExpectedMessageFlags::Contains, 4);
    UI->ViewState.bAuthenticated = true;
    UI->ViewState.CurrentStep = TEXT("DBA.Flow.InWorld");
    UI->ViewState.PageState = EDivineBeastsUIPageState::Ready;
    UI->ObserveWorldInputRouter(Router.Get());
    UI->bSystemMenuVisible = true;
    const auto UIOnly = UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode::UIOnly);
    Router->SetActiveUIInputConfig(UIOnly);
    UI->HandlePrimaryScreenClosed(TEXT("UI.Screen.Settings"));
    TestFalse(TEXT("直达设置关闭后撤销系统占用"), UI->bSystemMenuVisible);
    TestEqual(TEXT("真实Router恢复世界输入模式"), Router->GetActiveInputMode(), ECommonInputMode::All);
    UClass* MenuClass = LoadClass<UGamePlatformUIScreen>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_SystemMenu.WBP_DBA_UI_SystemMenu_C"));
    if (!TestNotNull(TEXT("已保存父菜单类"), MenuClass)) return false;
    TStrongObjectPtr<UGamePlatformUIScreen> Menu(NewObject<UGamePlatformUIScreen>(GetTransientPackage(), MenuClass));
    UI->ActiveSystemMenu = Menu.Get(); UI->bSystemMenuVisible = true;
    Router->SetActiveUIInputConfig(UIOnly);
    UI->HandlePrimaryScreenClosed(TEXT("UI.Screen.Settings"));
    TestTrue(TEXT("父菜单仍拥有输入"), UI->bSystemMenuVisible);
    TestEqual(TEXT("父菜单保留真实Menu输入"), Router->GetActiveInputMode(), ECommonInputMode::Menu);
    UI->ActiveSystemMenu.Reset();
    UI->OpeningSystemScreenId = TEXT("UI.Screen.Settings");
    UI->HandlePrimaryScreenClosed(TEXT("UI.Screen.Settings"));
    TestTrue(TEXT("旧页关闭不撤销正在打开的新系统页"), UI->bSystemMenuVisible);
    UI->OpeningSystemScreenId = NAME_None;
    UI->HandlePrimaryScreenClosed(TEXT("UI.Screen.Settings"));
    TestFalse(TEXT("最后系统页退出后恢复"), UI->bSystemMenuVisible);
    TestEqual(TEXT("等待结束后Router恢复世界输入"), Router->GetActiveInputMode(), ECommonInputMode::All);
    UI->ObserveWorldInputRouter(nullptr);
    return true;
}

/** 使用真实VM页面代次入口，证明失活/重进后的旧音量回调无法重建旧投影，也不改显示预览期限。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAudioSettingsLifecycleTest,
    "DivineBeasts.UI.Settings.AudioPageLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAudioSettingsLifecycleTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsSystemSettingsViewModel> VM(NewObject<UDivineBeastsSystemSettingsViewModel>());
    VM->BeginPage();
    const int32 OldGeneration = VM->GetPageGeneration();
    VM->DeviceSnapshot.Revision = 37;
    VM->PreviewDeadlineSeconds = 123.0;
    FGamePlatformSFXUserVolumes Volumes; Volumes.Effects = 0.3f; Volumes.Music = 0.0f; Volumes.Revision = 2;
    VM->HandleAudioSettingsChanged(Volumes, OldGeneration);
    TestEqual(TEXT("当前页面接受音频变化"), VM->AudioSnapshot.Effects, 0.3f);
    TestEqual(TEXT("音频不改设备快照"), VM->DeviceSnapshot.Revision, static_cast<int64>(37));
    TestEqual(TEXT("音频不延长设备预览期限"), VM->PreviewDeadlineSeconds, 123.0);
    VM->EndPage();
    const int32 EndRevision = VM->GetRevision();
    Volumes.Effects = 0.8f;
    VM->HandleAudioSettingsChanged(Volumes, OldGeneration);
    TestEqual(TEXT("失活拒绝迟到音频事件"), VM->GetRevision(), EndRevision);
    TestFalse(TEXT("失活撤销音频可编辑资格"), VM->bAudioSettingsAvailable);
    TestFalse(TEXT("失活清音频订阅句柄"), VM->AudioSubscription.IsValid());
    VM->BeginPage();
    const int32 NewGeneration = VM->GetPageGeneration();
    const int32 NewRevision = VM->GetRevision();
    VM->HandleAudioSettingsChanged(Volumes, OldGeneration);
    TestEqual(TEXT("新页拒绝旧代次"), VM->GetRevision(), NewRevision);
    VM->HandleAudioSettingsChanged(Volumes, NewGeneration);
    TestEqual(TEXT("新页接受新代次"), VM->AudioSnapshot.Effects, 0.8f);
    VM->EndPage();
    return true;
}
#endif
