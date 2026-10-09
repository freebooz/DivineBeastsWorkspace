#include "Tests/DivineBeastsUIReentryObserver.h"

#if WITH_EDITORONLY_DATA
// 两个反射函数与UHT类型采用同一EditorOnlyData条件；类型实现不依赖测试是否被过滤执行。
void UDivineBeastsUIReentryObserver::HandleStateChanged(int32 Revision, int32 PageGeneration)
{
    (void)Revision; (void)PageGeneration;
    ++StateNotifications;
    auto Action = MoveTemp(StateAction);
    if (Action) Action();
}
void UDivineBeastsUIReentryObserver::HandleCommandCompleted(FGuid RequestId, FName ErrorCode)
{
    (void)ErrorCode;
    ++CommandNotifications;
    LastCompletedRequestId = RequestId;
    auto Action = MoveTemp(CommandAction);
    if (Action) Action(RequestId);
}
#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"
#include "Screens/Connection/DivineBeastsErrorReconnectScreen.h"
#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Components/TextBlock.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

// 真实CommonUI激活通知可同步失活：派生旧激活栈不能重新订阅VM，也不能刷新StageText。
// 测试只分配Transient原生页/文本/VM，使用引擎受控抽象分配作用域；不修改类标志或资产。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLoadingTravelActivationReentryTest,
    "DivineBeasts.UI.LoadingTravel.ActivationObserverDeactivates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsLoadingTravelActivationReentryTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsLoadingTravelScreen> Screen(NewObject<UDivineBeastsLoadingTravelScreen>());
    const TStrongObjectPtr<UDivineBeastsLoadingViewModel> ViewModel(NewObject<UDivineBeastsLoadingViewModel>());
    const TStrongObjectPtr<UTextBlock> Stage(NewObject<UTextBlock>(Screen.Get()));
    FDelegateHandle ActivationHandle;
    ON_SCOPE_EXIT
    {
        Screen->OnActivated().Remove(ActivationHandle);
        Screen->DeactivateWidget();
        Screen->InitializeActivatableViewModel(nullptr);
    };
    auto* StageProperty = FindFProperty<FObjectPropertyBase>(Screen->GetClass(), TEXT("StageText"));
    if (!TestNotNull(TEXT("真实BindWidget StageText字段存在"), StageProperty)) return false;
    StageProperty->SetObjectPropertyValue_InContainer(Screen.Get(), Stage.Get());
    const FText Sentinel = FText::FromString(TEXT("失活页保持原文本"));
    Stage->SetText(Sentinel);
    Screen->InitializeActivatableViewModel(ViewModel.Get());
    bool bActivationObserved = false;
    ActivationHandle = Screen->OnActivated().AddLambda([&bActivationObserved, Selected = Screen.Get()]()
    {
        bActivationObserved = true;
        Selected->DeactivateWidget();
    });
    Screen->ActivateWidget(); // 实际CommonUI NativeOnActivated通知，未手工调用派生函数或广播模拟结果。
    TestTrue(TEXT("真实激活通知触发观察者"), bActivationObserved);
    TestFalse(TEXT("观察者同步失活后页面保持失活"), Screen->IsActivated());
    TestFalse(TEXT("旧激活栈不会启动失活VM页面"), ViewModel->IsPageActive());
    TestFalse(TEXT("失活页不残留任何VM动态订阅"), ViewModel->OnViewStateChanged.GetAllObjects().Contains(Screen.Get()));
    TestTrue(TEXT("旧激活栈不会改写失活页StageText"), Stage->GetText().EqualTo(Sentinel));
    ViewModel->MarkStateChanged();
    TestTrue(TEXT("失活VM后续真实状态通知也不改写文本"), Stage->GetText().EqualTo(Sentinel));
    return true;
}

// 激活中合法替换VM必须撤旧订阅、启动新页面代次并读取真实LoadingService快照。
// 只注入Transient文本字段；令牌/换VM/激活均走真实公开API，作用域退出解绑并释放令牌。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsLoadingTravelViewModelReplacementTest,
    "DivineBeasts.UI.LoadingTravel.ActiveViewModelReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsLoadingTravelViewModelReplacementTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsLoadingTravelScreen> Screen(NewObject<UDivineBeastsLoadingTravelScreen>());
    const TStrongObjectPtr<UDivineBeastsLoadingViewModel> Previous(NewObject<UDivineBeastsLoadingViewModel>());
    const TStrongObjectPtr<UDivineBeastsLoadingViewModel> Incoming(NewObject<UDivineBeastsLoadingViewModel>());
    const TStrongObjectPtr<UGamePlatformLoadingScreenService> Service(NewObject<UGamePlatformLoadingScreenService>());
    const TStrongObjectPtr<UTextBlock> Stage(NewObject<UTextBlock>(Screen.Get()));
    ON_SCOPE_EXIT
    {
        Screen->DeactivateWidget();
        Screen->InitializeActivatableViewModel(nullptr);
        Service->ReleaseAll();
    };
    auto* StageProperty = FindFProperty<FObjectPropertyBase>(Screen->GetClass(), TEXT("StageText"));
    if (!TestNotNull(TEXT("真实BindWidget StageText字段存在"), StageProperty)) return false;
    StageProperty->SetObjectPropertyValue_InContainer(Screen.Get(), Stage.Get());
    const FText InitialStage = FText::FromString(TEXT("实际加载初始阶段"));
    const FGamePlatformLoadingToken Token = Service->AcquireToken(InitialStage, -1.0f);
    if (!TestTrue(TEXT("真实Loading令牌受理"), Token.IsValid())) return false;
    Previous->InitializeLoadingService(Service.Get());
    Incoming->InitializeLoadingService(Service.Get());
    Screen->InitializeActivatableViewModel(Previous.Get());
    Screen->ActivateWidget();
    TestTrue(TEXT("真实激活后的初始快照可见"), Stage->GetText().EqualTo(InitialStage));
    Screen->InitializeActivatableViewModel(Incoming.Get());
    TestEqual(TEXT("合法替换保持新VM身份"), Screen->GetLoadingViewModel(), Incoming.Get());
    TestFalse(TEXT("旧VM页面生命周期已经结束"), Previous->IsPageActive());
    TestTrue(TEXT("新VM页面生命周期已经开始"), Incoming->IsPageActive());
    TestFalse(TEXT("旧VM动态委托精确移除页面"), Previous->OnViewStateChanged.GetAllObjects().Contains(Screen.Get()));
    TestEqual(TEXT("新VM仅有平台统一订阅，不存在派生双订阅"), Incoming->OnViewStateChanged.GetAllObjects().Num(), 1);
    const FText Sentinel = FText::FromString(TEXT("旧VM通知不应覆盖"));
    Stage->SetText(Sentinel);
    Previous->MarkStateChanged();
    TestTrue(TEXT("旧VM真实状态通知不触发当前页刷新"), Stage->GetText().EqualTo(Sentinel));
    const FText UpdatedStage = FText::FromString(TEXT("实际加载下一阶段"));
    TestTrue(TEXT("真实令牌阶段更新"), Service->UpdateToken(Token, UpdatedStage, -1.0f));
    TestTrue(TEXT("新VM真实快照事件更新StageText"), Stage->GetText().EqualTo(UpdatedStage));
    Screen->DeactivateWidget();
    TestFalse(TEXT("失活后新VM动态订阅也被撤销"), Incoming->OnViewStateChanged.GetAllObjects().Contains(Screen.Get()));
    TestFalse(TEXT("失活后LoadingService无VM订阅残留"), Service->OnSnapshotChanged.IsBound());
    return true;
}

#if WITH_EDITORONLY_DATA
namespace
{
/** 真实Engine/LocalPlayer/项目Owner与Native错误页；不Initialize Owner/创建世界/登录或资产。 */
struct FDivineBeastsErrorNativeFixture
{
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UDivineBeastsUIClientSubsystem> Owner;
    TStrongObjectPtr<UDivineBeastsErrorReconnectScreen> Screen;
    TStrongObjectPtr<UButton> Button;
    TStrongObjectPtr<UTextBlock> Text;
    bool Initialize(FAutomationTestBase& Test)
    {
        if (!Test.TestNotNull(TEXT("错误页夹具需要真实Engine Outer"), GEngine)) return false;
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        Owner.Reset(NewObject<UDivineBeastsUIClientSubsystem>(Player.Get()));
        Screen.Reset(NewObject<UDivineBeastsErrorReconnectScreen>());
        Button.Reset(NewObject<UButton>(Screen.Get()));
        Text.Reset(NewObject<UTextBlock>(Screen.Get()));
        auto* ButtonProperty = FindFProperty<FObjectPropertyBase>(Screen->GetClass(), TEXT("RetryButton"));
        auto* TextProperty = FindFProperty<FObjectPropertyBase>(Screen->GetClass(), TEXT("ErrorText"));
        if (!Test.TestNotNull(TEXT("真实RetryButton字段存在"), ButtonProperty) ||
            !Test.TestNotNull(TEXT("真实ErrorText字段存在"), TextProperty)) return false;
        ButtonProperty->SetObjectPropertyValue_InContainer(Screen.Get(), Button.Get());
        TextProperty->SetObjectPropertyValue_InContainer(Screen.Get(), Text.Get());
        return true;
    }
    ~FDivineBeastsErrorNativeFixture()
    {
        if (Screen.IsValid()) { Screen->DeactivateWidget(); Screen->InitializeActivatableViewModel(nullptr); }
        if (Owner.IsValid()) Owner->Deinitialize();
    }
};
/** 只给Transient UI快照设置测试按钮输入资格；Owner仍无Contract，真实命令必须同步失败，绝非后端授权/成功替身。 */
bool AllowFixtureRetry(FAutomationTestBase& Test, UDivineBeastsUIViewModel& ViewModel)
{
    auto* StateProperty = FindFProperty<FStructProperty>(ViewModel.GetClass(), TEXT("State"));
    if (!Test.TestNotNull(TEXT("真实VM只读快照字段存在"), StateProperty)) return false;
    auto* State = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(&ViewModel);
    State->AllowedCommands = {FName(TEXT("Retry"))}; State->bBusy = false;
    ViewModel.MarkStateChanged();
    return true;
}
}

// 真实CommonUI激活观察者同步失活，项目父链不得重挂Owner State/按钮/VM状态或命令事件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsErrorActivationReentryTest,
    "DivineBeasts.UI.ErrorReconnect.ActivationObserverDeactivates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsErrorActivationReentryTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsErrorNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> ViewModel(NewObject<UDivineBeastsUIViewModel>());
    ViewModel->InitializeForScreen(Fixture.Owner.Get(), TEXT("UI.Screen.ErrorReconnect"));
    Fixture.Screen->InitializeActivatableViewModel(ViewModel.Get());
    bool bObserved = false;
    const FDelegateHandle Handle = Fixture.Screen->OnActivated().AddLambda([&bObserved, Screen = Fixture.Screen.Get()]
    { bObserved = true; Screen->DeactivateWidget(); });
    ON_SCOPE_EXIT { Fixture.Screen->OnActivated().Remove(Handle); };
    Fixture.Screen->ActivateWidget();
    TestTrue(TEXT("真实CommonUI通知确已执行"), bObserved);
    TestFalse(TEXT("页面保持观察者关闭结果"), Fixture.Screen->IsActivated());
    TestFalse(TEXT("父链不复活Owner状态订阅"), Fixture.Owner->OnStateChanged().IsBoundToObject(ViewModel.Get()));
    TestFalse(TEXT("失活后按钮不重挂"), Fixture.Button->OnClicked.IsBound());
    TestFalse(TEXT("失活后VM状态不重挂"), ViewModel->OnViewStateChanged.GetAllObjects().Contains(Fixture.Screen.Get()));
    TestFalse(TEXT("失活后VM命令不重挂"), ViewModel->OnCommandCompleted.GetAllObjects().Contains(Fixture.Screen.Get()));
    return true;
}

// 激活期间合法VM替换，父链Owner订阅与Error命令订阅都必须精确拆旧接新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsErrorViewModelReplacementTest,
    "DivineBeasts.UI.ErrorReconnect.ActiveViewModelReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsErrorViewModelReplacementTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsErrorNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(NewObject<UDivineBeastsUIViewModel>());
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Incoming(NewObject<UDivineBeastsUIViewModel>());
    Previous->InitializeForScreen(Fixture.Owner.Get(), TEXT("UI.Screen.ErrorReconnect"));
    Incoming->InitializeForScreen(Fixture.Owner.Get(), TEXT("UI.Screen.ErrorReconnect"));
    Fixture.Screen->InitializeActivatableViewModel(Previous.Get()); Fixture.Screen->ActivateWidget();
    TestTrue(TEXT("父链实际订阅旧Owner状态"), Fixture.Owner->OnStateChanged().IsBoundToObject(Previous.Get()));
    Fixture.Screen->InitializeActivatableViewModel(Incoming.Get());
    TestFalse(TEXT("旧Owner状态订阅被精确移除"), Fixture.Owner->OnStateChanged().IsBoundToObject(Previous.Get()));
    TestTrue(TEXT("新Owner状态订阅建立"), Fixture.Owner->OnStateChanged().IsBoundToObject(Incoming.Get()));
    TestFalse(TEXT("旧VM状态委托被移除"), Previous->OnViewStateChanged.GetAllObjects().Contains(Fixture.Screen.Get()));
    TestFalse(TEXT("旧VM命令委托被移除"), Previous->OnCommandCompleted.GetAllObjects().Contains(Fixture.Screen.Get()));
    TestEqual(TEXT("新VM只有平台唯一状态订阅"), Incoming->OnViewStateChanged.GetAllObjects().Num(), 1);
    TestEqual(TEXT("新VM只有本页唯一命令订阅"), Incoming->OnCommandCompleted.GetAllObjects().Num(), 1);
    Fixture.Screen->DeactivateWidget();
    TestFalse(TEXT("失活移除新Owner状态订阅"), Fixture.Owner->OnStateChanged().IsBoundToObject(Incoming.Get()));
    TestFalse(TEXT("失活移除新命令订阅"), Incoming->OnCommandCompleted.IsBound());
    return true;
}

// 真实无Contract Owner同步拒绝Retry：状态通知中关闭并建立后继页，旧返回栈不得尾写新请求。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsErrorRetrySuccessorTest,
    "DivineBeasts.UI.ErrorReconnect.SynchronousRetryKeepsSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsErrorRetrySuccessorTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsErrorNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(NewObject<UDivineBeastsUIViewModel>());
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Incoming(NewObject<UDivineBeastsUIViewModel>());
    const TStrongObjectPtr<UDivineBeastsUIReentryObserver> Observer(NewObject<UDivineBeastsUIReentryObserver>());
    Previous->InitializeForScreen(Fixture.Owner.Get(), TEXT("UI.Screen.ErrorReconnect"));
    Incoming->InitializeForScreen(Fixture.Owner.Get(), TEXT("UI.Screen.ErrorReconnect"));
    Fixture.Screen->InitializeActivatableViewModel(Previous.Get()); Fixture.Screen->ActivateWidget();
    if (!AllowFixtureRetry(*this, *Previous)) return false;
    Previous->OnViewStateChanged.AddDynamic(Observer.Get(), &UDivineBeastsUIReentryObserver::HandleStateChanged);
    Previous->OnCommandCompleted.AddDynamic(Observer.Get(), &UDivineBeastsUIReentryObserver::HandleCommandCompleted);
    Incoming->OnCommandCompleted.AddDynamic(Observer.Get(), &UDivineBeastsUIReentryObserver::HandleCommandCompleted);
    ON_SCOPE_EXIT
    {
        Observer->StateAction = nullptr; Observer->CommandAction = nullptr;
        Previous->OnViewStateChanged.RemoveAll(Observer.Get());
        Previous->OnCommandCompleted.RemoveAll(Observer.Get()); Incoming->OnCommandCompleted.RemoveAll(Observer.Get());
    };
    Observer->StateAction = [this, &Fixture, Selected = Incoming.Get()]
    {
        Fixture.Screen->DeactivateWidget();
        Fixture.Screen->InitializeActivatableViewModel(Selected); Fixture.Screen->ActivateWidget();
        AllowFixtureRetry(*this, *Selected);
    };
    Fixture.Button->OnClicked.Broadcast(); // 真实按钮委托为测试输入；完成结果由真实VM/Owner同步失败链产生。
    TestTrue(TEXT("实际Retry状态通知触发重入"), Observer->StateNotifications > 0);
    TestTrue(TEXT("实际同步失败有非空命令身份"), Observer->LastCompletedRequestId.IsValid());
    TestEqual(TEXT("无Contract真实错误保持"), Previous->GetLastCommandErrorCode(), FName(TEXT("UI.Command.Invalid")));
    TestTrue(TEXT("后继页面保持激活"), Fixture.Screen->IsActivated());
    TestEqual(TEXT("后继VM身份保持"), Fixture.Screen->GetViewModel(), static_cast<UGamePlatformViewModelBase*>(Incoming.Get()));
    TestTrue(TEXT("旧Retry返回不能将后继按钮误标忙碌"), Fixture.Button->GetIsEnabled());
    const int32 Before = Observer->CommandNotifications;
    Fixture.Button->OnClicked.Broadcast();
    TestEqual(TEXT("后继仍可实际提交一次Retry"), Observer->CommandNotifications, Before + 1);
    TestTrue(TEXT("后继同步失败终态不遗留在飞"), Fixture.Button->GetIsEnabled());
    return true;
}
#endif

// 黑屏回归：流程真实会路由到LoadingTravel/ErrorReconnect，规划软路径不能代替可实例化页面。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRequiredFlowScreensTest,
    "DivineBeasts.UI.Delivery.RequiredTransferScreens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRequiredFlowScreensTest::RunTest(const FString&)
{
    for (const FName Id : {FName(TEXT("UI.Screen.LoadingTravel")), FName(TEXT("UI.Screen.ErrorReconnect"))})
    {
        const auto* Surface = FDivineBeastsUIScreenCatalog::Find(Id);
        if (!TestNotNull(TEXT("真实流程页面已登记"), Surface)) continue;
        UClass* Class = FSoftClassPath(Surface->WidgetClassPath).TryLoadClass<UGamePlatformUIScreen>();
        if (!TestNotNull(*FString::Printf(TEXT("%s必须交付可加载类"), *Id.ToString()), Class)) continue;
        TestFalse(TEXT("交付页面可实例化"), Class->HasAnyClassFlags(CLASS_Abstract));
        TestTrue(TEXT("加载与错误页的租约跨地图保留，直到业务关闭"), Surface->bSurvivesTravel);
        UClass* Expected = Id == TEXT("UI.Screen.LoadingTravel")
            ? UDivineBeastsLoadingTravelScreen::StaticClass() : UDivineBeastsErrorReconnectScreen::StaticClass();
        TestTrue(TEXT("页面继承现有第三层事件驱动父类"), Class->IsChildOf(Expected));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIScreenInventoryTest,
    "DivineBeasts.UI.ScreenInventory",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIScreenInventoryTest::RunTest(const FString&)
{
    const TArray<FDivineBeastsUISurfaceDescriptor>& Surfaces =
        FDivineBeastsUIScreenCatalog::GetSurfaces();

    // 十大业务域登记11个公共Screen、4个HUD与1个通知，共16个非竞技表面。
    // 新登记的社交与运营软路径不表示对应蓝图已通过Monolith交付。
    TestEqual(TEXT("公共非竞技UI表面数量"), Surfaces.Num(), 16);

    TSet<FName> Unique;
    for (const FDivineBeastsUISurfaceDescriptor& Surface : Surfaces)
    {
        TestTrue(TEXT("SurfaceId有效"), !Surface.SurfaceId.IsNone());
        TestFalse(TEXT("SurfaceId唯一"), Unique.Contains(Surface.SurfaceId));
        Unique.Add(Surface.SurfaceId);

        TestTrue(
            TEXT("公共项目UI软资源路径必须归第三层DBAUIPack_Core内容包"),
            Surface.WidgetClassPath.StartsWith(TEXT("/DBAUIPack_Core/")));
        TestTrue(
            TEXT("移动端变体尚未交付时必须保持空路径并回退公共资产"),
            Surface.MobileWidgetClassPath.IsEmpty());
    }

    for (const FName Required : {
        FName(TEXT("UI.Screen.Boot")),
        FName(TEXT("UI.Screen.Login")),
        FName(TEXT("UI.Screen.CharacterCreate")),
        FName(TEXT("UI.Screen.CharacterSelect")),
        FName(TEXT("UI.Screen.LoadingTravel")),
        FName(TEXT("UI.Screen.Inventory")),
        FName(TEXT("UI.Screen.Quest")),
         FName(TEXT("UI.Screen.Social")),
         FName(TEXT("UI.Screen.LiveOps")),
        FName(TEXT("UI.HUD.OpenWorld")),
        FName(TEXT("UI.HUD.VillageMain")),
        FName(TEXT("UI.HUD.TutorialGuidance")),
        FName(TEXT("UI.HUD.TrainingControls")),
        FName(TEXT("UI.Notification.Toast"))
    })
    {
        TestNotNull(
            *FString::Printf(TEXT("Required surface %s"), *Required.ToString()),
            FDivineBeastsUIScreenCatalog::Find(Required));
    }

    const FDivineBeastsUISurfaceDescriptor* Boot =
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.Boot"));
    TestNotNull(TEXT("启动封面目录项必须存在"), Boot);
    if (Boot)
    {
        TestTrue(
            TEXT("项目UI软路径必须归DBAUIPack_Core规划挂载点"),
            Boot->WidgetClassPath.StartsWith(TEXT("/DBAUIPack_Core/")));
        TestFalse(
            TEXT("不得继续使用不存在的旧DivineBeastsUI挂载点"),
            Boot->WidgetClassPath.StartsWith(TEXT("/DivineBeastsUI/")));
    }

    TestNull(
        TEXT("一期未批准Settings页面不应被擅自创建"),
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.Settings")));
    TestNull(
        TEXT("角色列表必须作为CharacterSelect内部区域，不再维护独立页面"),
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.CharacterRoster")));

    TestNull(
        TEXT("公共DBAClient不得注册竞技匹配页面"),
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.Matchmaking")));
    TestNull(
        TEXT("公共DBAClient不得注册竞技HUD"),
        FDivineBeastsUIScreenCatalog::Find(TEXT("UI.HUD.Arena")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUICommandContractTest,
    "DivineBeasts.UI.CommandContract",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUICommandContractTest::RunTest(const FString&)
{
    FDivineBeastsUICommand Login;
    Login.RequestId = FGuid::NewGuid();
    Login.Type = EDivineBeastsUICommandType::Login;

    FString Error;
    TestFalse(TEXT("空登录命令拒绝"), Login.IsValid(Error));

    Login.LoginName = TEXT("test-user");
    Login.Password = TEXT("transient-only");
    Error.Reset();
    TestTrue(TEXT("瞬时登录参数有效"), Login.IsValid(Error));

    FDivineBeastsUICommand Create;
    Create.RequestId = FGuid::NewGuid();
    Create.Type = EDivineBeastsUICommandType::CreateCharacter;
    Create.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Create.CharacterName = TEXT("TestCharacter");
    Error.Reset();
    TestTrue(TEXT("角色创建意图有效"), Create.IsValid(Error));

    FDivineBeastsUICommand Select;
    Select.RequestId = FGuid::NewGuid();
    Select.Type =
        EDivineBeastsUICommandType::SelectPersistentCharacter;
    Error.Reset();
    TestFalse(TEXT("持久角色选择必须携带CharacterId"), Select.IsValid(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUICommandCompletionLifetimeTest,
    "DivineBeasts.UI.CommandCompletionLifetime",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUICommandCompletionLifetimeTest::RunTest(const FString&)
{
    UDivineBeastsUIViewModel* ViewModel =
        NewObject<UDivineBeastsUIViewModel>();
    TestNotNull(TEXT("命令终态测试必须创建ViewModel"), ViewModel);
    if (!ViewModel)
    {
        return false;
    }

    ViewModel->BeginPage();
    const int32 SubmittedRevision = ViewModel->GetRevision();
    const int32 SubmittedPageGeneration = ViewModel->GetPageGeneration();
    const FGuid RequestId = FGuid::NewGuid();
    ViewModel->PendingCommands.Add(RequestId);

    // 模拟提交后业务忙碌状态先到达；这会增加Revision，但页面代次和请求身份未变。
    ViewModel->MarkStateChanged();

    FDivineBeastsUICommandResult Result;
    Result.RequestId = RequestId;
    Result.bAccepted = false;
    Result.ErrorCode = TEXT("InvalidCredentials");
    ViewModel->HandleCommandResult(
        SubmittedPageGeneration,
        Result);

    TestEqual(
        TEXT("同页面内状态先变化也必须交付命令终态"),
        ViewModel->GetLastCommandErrorCode(),
        FName(TEXT("InvalidCredentials")));
    TestFalse(
        TEXT("命令终态到达后必须清理待处理请求"),
        ViewModel->PendingCommands.Contains(RequestId));
    ViewModel->EndPage();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUIRoutingPolicyTest,
    "DivineBeasts.UI.RoutingPolicy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUIRoutingPolicyTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    TestEqual(
        TEXT("流程尚未产生步骤时必须路由到Boot"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.Boot")));

    State.CurrentStep = TEXT("DBA.Flow.Authentication");
    TestEqual(
        TEXT("认证页面路由到Login"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.Login")));

    State.PageState = EDivineBeastsUIPageState::Error;
    // 网络传输终态失败可与尚未清除的加载快照同批到达；错误入口必须可见。
    State.Loading.bIsLoading = true;
    TestEqual(
        TEXT("错误优先进入重连/错误页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.ErrorReconnect")));

    State.PageState = EDivineBeastsUIPageState::Ready;
    State.Loading.bIsLoading = true;
    TestEqual(
        TEXT("Loading优先进入真实加载页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.LoadingTravel")));

    State.Loading.bIsLoading = false;
    State.CurrentStep = TEXT("DBA.Flow.LoadRoster");
    State.Characters.Reset();
    TestEqual(
        TEXT("无持久角色时进入角色创建页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.CharacterCreate")));

    FDivineBeastsUICharacterItem Character;
    Character.CharacterId = TEXT("character-test-001");
    State.Characters.Add(Character);
    TestEqual(
        TEXT("存在持久角色时直接进入角色选择页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.CharacterSelect")));

    // 已有角色仍可打开创建表单；仅本地路由偏好，验证节点、忙碌和注销不能被换页绕过。
    State.CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    State.bAuthenticated = true;
    State.AllowedCommands = {TEXT("CreateCharacter"), TEXT("SelectPersistentCharacter")};
    TestTrue(TEXT("已有角色也允许进入创建页"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    TestEqual(TEXT("创建偏好不能被已有角色列表覆盖"), FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterCreate")));
    TestTrue(TEXT("取消创建可返回真实角色列表"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
    State.bBusy = true;
    TestFalse(TEXT("提交中拒绝新的换页命令"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    TestEqual(TEXT("忙碌事件不把已打开的创建页切回选择页"), FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterCreate")));
    State.bBusy = false;
    State.Characters.Reset();
    TestFalse(TEXT("无角色时拒绝返回空选择页"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
    TestFalse(TEXT("拒绝借角色换页入口打开任意页面"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.Inventory")));
    State.Characters.Add(Character);
    State.bAuthenticated = false;
    TestFalse(TEXT("注销后旧创建命令无效"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    State.bAuthenticated = true;
    State.CurrentStep = TEXT("DBA.Flow.ValidateSelection");
    TestFalse(TEXT("权威验证节点不允许角色入口换页"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    TestEqual(TEXT("创建偏好不能覆盖权威验证页面"), FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterSelect")));
    State.PageState = EDivineBeastsUIPageState::Submitting;
    TestEqual(
        TEXT("服务端权威验证期间保持角色选择页并由页面显示提交遮罩"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.CharacterSelect")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsUILocalizationTest,
    "DivineBeasts.UI.Localization",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsUILocalizationTest::RunTest(const FString&)
{
    TestFalse(
        TEXT("已知错误必须映射本地化文本"),
        FDivineBeastsUILocalization::ErrorCodeToText(
            TEXT("InvalidCredentials")).IsEmpty());
    TestFalse(
        TEXT("未知错误也必须安全降级"),
        FDivineBeastsUILocalization::ErrorCodeToText(
            TEXT("Internal.Technical.Error")).IsEmpty());
    return true;
}

#endif
