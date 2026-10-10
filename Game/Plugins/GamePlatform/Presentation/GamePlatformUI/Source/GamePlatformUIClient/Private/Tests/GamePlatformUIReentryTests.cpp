// 本文件属于平台客户端UI生命周期回归；真实引擎对象/动态委托验证结果内关闭与VM自闭，不生成视觉资产。
// 前置条件/输入/失败意义与未执行UE边界见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/GamePlatformUIReentryFixture.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIRootCloseReentryTest,
    "GamePlatform.UI.Lifecycle.RootReplacementAndFailureNotificationClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIRootCloseReentryTest::RunTest(const FString&)
{
    // LocalPlayer与Viewport的ClassWithin为Engine；仅用真实GEngine作Outer，缺引擎明确失败。
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) { return false; }
    FScopedAllowAbstractClassAllocation AllowAbstract; // 仅Transient基础Screen；公开Install仍使用具体夹具Class。
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    // 让Controller执行真实PostInitialize，结构读取失败时也由作用域收回测试世界。
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    const TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    // 引擎World/Viewport关系为Transient夹具；不加载地图、不创建实际窗口。
    auto* ViewportWorld = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("World"));
    if (!TestNotNull(TEXT("真实Viewport World字段"), ViewportWorld)) return false;
    ViewportWorld->SetObjectPropertyValue_InContainer(Viewport.Get(), World.Get()); Player->ViewportClient = Viewport.Get();
    auto* Controller = World->SpawnActor<APlayerController>(); Controller->SetPlayer(Player.Get()); Player->PlayerController = Controller;
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Manager(NewObject<UGamePlatformUIManagerSubsystem>(Player.Get()));
    const TStrongObjectPtr<UGamePlatformUIReentryRootFixture> Root(NewObject<UGamePlatformUIReentryRootFixture>());
    const TStrongObjectPtr<UGamePlatformUIScreen> Screen(NewObject<UGamePlatformUIScreen>());
    const TStrongObjectPtr<UCommonActivatableWidgetStack> Stack(NewObject<UCommonActivatableWidgetStack>());
    const TStrongObjectPtr<UGamePlatformUIReentryObserver> Observer(NewObject<UGamePlatformUIReentryObserver>());
    Screen->InitializeScreen(TEXT("CloseDuringReplace"), nullptr, NAME_None, EGamePlatformUIInputMode::UIOnly, EGamePlatformUIPausePolicy::Never);
    Manager->RootLayout = Root.Get(); Manager->ScreenStacks.Add(Screen.Get(), Stack.Get());
    Observer->Manager = Manager.Get(); Manager->OnScreenClosed.AddDynamic(Observer.Get(), &UGamePlatformUIReentryObserver::HandleClosed);
    TestFalse(TEXT("Closed观察者关停后替根必须失败且不解引用空旧布局"), Manager->InstallRootLayoutClass(UGamePlatformUIReentryRootFixture::StaticClass()));
    TestEqual(TEXT("只通知原屏关闭一次"), Observer->Notifications, 1);
    TestTrue(TEXT("服务保持关闭"), Manager->bClosing);
    TestNull(TEXT("关闭后未重新发布新布局"), Manager->GetRootLayout());
    TestEqual(TEXT("关闭后屏所有权为零"), Manager->ScreenStacks.Num(), 0);
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> ResultManager(NewObject<UGamePlatformUIManagerSubsystem>(Player.Get()));
    Observer->Manager = ResultManager.Get(); Observer->Notifications = 0;
    ResultManager->OnScreenOpenFailed.AddDynamic(Observer.Get(), &UGamePlatformUIReentryObserver::HandleFailed);
    ResultManager->OpenScreenAsync(TEXT("UnregisteredScreen"), nullptr);
    TestEqual(TEXT("真实开屏拒绝通知可同步关停"), Observer->Notifications, 1);
    TestTrue(TEXT("结果通知关停后保持关闭"), ResultManager->bClosing);
    TestEqual(TEXT("结果通知不遗留等待请求"), ResultManager->PendingRequests.Num(), 0);
    Player->PlayerController = nullptr;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIViewModelChangeReentryTest,
    "GamePlatform.UI.Lifecycle.ViewModelChangeSelfCloseAndNestedChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIViewModelChangeReentryTest::RunTest(const FString&)
{
    // LocalPlayer与Viewport的ClassWithin为Engine；仅用真实GEngine作Outer，缺引擎明确失败。
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) { return false; }
    const TStrongObjectPtr<UGamePlatformUIReentryWidget> Widget(NewObject<UGamePlatformUIReentryWidget>());
    const TStrongObjectPtr<UGamePlatformUIReentryViewModel> Old(NewObject<UGamePlatformUIReentryViewModel>());
    const TStrongObjectPtr<UGamePlatformUIReentryViewModel> New(NewObject<UGamePlatformUIReentryViewModel>());
    Widget->InitializeActivatableViewModel(Old.Get()); Widget->ActivateWidget();
    New->BeganHook = [&] { Widget->DeactivateWidget(); };
    Widget->InitializeActivatableViewModel(New.Get());
    TestFalse(TEXT("新VM BeginPage自闭后控件失活"), Widget->IsActivated());
    TestFalse(TEXT("新VM自闭结束页面代次"), New->IsPageActive());
    TestFalse(TEXT("新VM自闭返回后不得重新绑定状态事件"), New->OnViewStateChanged.Contains(Widget.Get(), FName(TEXT("HandleViewModelStateChanged"))));
    New->BeganHook = nullptr;
    Widget->InitializeActivatableViewModel(Old.Get()); Widget->ActivateWidget();
    Old->EndedHook = [&] { Widget->DeactivateWidget(); };
    Widget->InitializeActivatableViewModel(New.Get());
    TestFalse(TEXT("旧EndPage自闭不能开始新VM"), New->IsPageActive());
    TestFalse(TEXT("旧EndPage自闭后新VM无订阅"), New->OnViewStateChanged.Contains(Widget.Get(), FName(TEXT("HandleViewModelStateChanged"))));
    Old->EndedHook = nullptr;
    const TStrongObjectPtr<UGamePlatformUIReentryViewModel> Nested(NewObject<UGamePlatformUIReentryViewModel>());
    Widget->InitializeActivatableViewModel(Old.Get()); Widget->ActivateWidget();
    Old->EndedHook = [&] { Widget->InitializeActivatableViewModel(Nested.Get()); };
    Widget->InitializeActivatableViewModel(New.Get());
    TestEqual(TEXT("旧EndPage嵌套替换优先于外层返回"), Widget->GetPlatformViewModel(), static_cast<UGamePlatformViewModelBase*>(Nested.Get()));
    TestTrue(TEXT("嵌套VM新页保持有效"), Nested->IsPageActive());
    Old->EndedHook = nullptr; Widget->RefreshHook = [&] { Widget->DeactivateWidget(); };
    Widget->InitializeActivatableViewModel(New.Get());
    TestFalse(TEXT("初始刷新自闭后不得保留事件"), New->OnViewStateChanged.Contains(Widget.Get(), FName(TEXT("HandleViewModelStateChanged"))));
    const int32 PreviousEvents = Widget->StateEvents; New->MarkStateChanged();
    TestEqual(TEXT("失活页面不接受迟到状态刷新"), Widget->StateEvents, PreviousEvents);
    return true;
}
#endif
