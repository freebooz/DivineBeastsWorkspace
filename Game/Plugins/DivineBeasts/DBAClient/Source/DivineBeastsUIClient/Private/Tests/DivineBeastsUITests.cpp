// 项目客户端UI回归：验证只读快照、页面路由、命令资格及事件生命周期；仅自动化测试使用。
// 瞬态原生控件和测试快照不写资产、不登录、不连接后端，不代表世界准入或客户端视觉验收。
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
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"
#include "Screens/Connection/DivineBeastsErrorReconnectScreen.h"
#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"
#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"
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

    // 十大业务域登记12个公共Screen（含真实世界系统菜单）、4个HUD与1个通知，共17个非竞技表面。
    // 新登记的社交与运营软路径不表示对应蓝图已通过Monolith交付。
    TestEqual(TEXT("公共非竞技UI表面数量"), Surfaces.Num(), 17);

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

    TestNotNull(
        TEXT("已批准的真实设备设置页必须登记"),
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
        TEXT("空角色列表仍进入选择页，创建必须由用户显式打开"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State),
        FName(TEXT("UI.Screen.CharacterSelect")));

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
    TestTrue(TEXT("无角色也允许取消创建并返回空选择页"), FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
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

// 登录成功的CharacterEntry默认显示选择页，角色数量和创建资格不能隐式打开创建表单。
// 仅使用手工给定的只读快照；路由不得修改业务角色、选择标识或允许命令。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterEntryDefaultSelectionTest,
    "DivineBeasts.UI.RoutingPolicy.CharacterEntryDefaultsToSelect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterEntryDefaultSelectionTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    State.CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    State.PageState = EDivineBeastsUIPageState::Empty;
    State.bAuthenticated = true;
    State.AllowedCommands = {TEXT("CreateCharacter")};
    TestEqual(TEXT("已认证空角色入口默认选择页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State), FName(TEXT("UI.Screen.CharacterSelect")));
    TestEqual(TEXT("无效本地偏好回退选择页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.Inventory")), FName(TEXT("UI.Screen.CharacterSelect")));
    TestTrue(TEXT("用户可显式打开已允许的创建表单"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    TestEqual(TEXT("显式创建偏好才显示创建表单"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterCreate")));
    TestTrue(TEXT("显示页面不生成角色"), State.Characters.IsEmpty());
    TestTrue(TEXT("显示页面不生成业务选择"), State.SelectedCharacterId.IsEmpty());
    TestEqual(TEXT("显示页面不追加选择或世界命令"), State.AllowedCommands.Num(), 1);

    FDivineBeastsUICharacterItem Character;
    Character.CharacterId = TEXT("character-routing-fixture-001");
    State.Characters.Add(Character);
    State.PageState = EDivineBeastsUIPageState::Ready;
    TestEqual(TEXT("已有角色入口也默认选择页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State), FName(TEXT("UI.Screen.CharacterSelect")));
    return true;
}

// 取消草稿属于本地换页，不等同于提交角色选择；无角色、无选择命令仍应可返回。
// 忙碌快照保持已经选择的页面，新的换页意图仍须由独立资格门禁拒绝。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsEmptyRosterCreationCancelTest,
    "DivineBeasts.UI.RoutingPolicy.EmptyRosterCreationCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsEmptyRosterCreationCancelTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState State;
    State.CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    State.PageState = EDivineBeastsUIPageState::Empty;
    State.bAuthenticated = true;
    State.AllowedCommands = {TEXT("CreateCharacter")};
    TestTrue(TEXT("无选择提交权限也允许取消空角色创建"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
    TestEqual(TEXT("取消后的空选择页偏好不会再次打开创建表单"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterSelect")), FName(TEXT("UI.Screen.CharacterSelect")));
    State.AllowedCommands.Reset();
    TestTrue(TEXT("仅显示选择页不需要业务提交命令"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
    TestFalse(TEXT("缺少创建命令时仍不能打开创建表单"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    State.bBusy = true;
    TestEqual(TEXT("忙碌事件保持空选择页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterSelect")), FName(TEXT("UI.Screen.CharacterSelect")));
    TestEqual(TEXT("忙碌事件保持已显式打开的创建页"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(State, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterCreate")));
    TestFalse(TEXT("忙碌期间不能通过取消换页"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
    return true;
}

// 放开空角色取消后，认证、忙碌、加载、错误和权威节点仍逐项隔离；不允许任意页面ID。
// 每个反例都从合法空角色入口独立复制，避免上一个拒绝条件掩盖下一个条件的缺失。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterEntryNavigationGuardrailsTest,
    "DivineBeasts.UI.RoutingPolicy.CharacterEntryNavigationGuardrails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterEntryNavigationGuardrailsTest::RunTest(const FString&)
{
    FDivineBeastsUIViewState Entry;
    Entry.CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    Entry.PageState = EDivineBeastsUIPageState::Ready;
    Entry.bAuthenticated = true;
    Entry.AllowedCommands = {TEXT("CreateCharacter"), TEXT("SelectPersistentCharacter")};
    FDivineBeastsUIViewState Busy = Entry; Busy.bBusy = true;
    FDivineBeastsUIViewState Loading = Entry; Loading.Loading.bIsLoading = true;
    FDivineBeastsUIViewState Error = Entry; Error.PageState = EDivineBeastsUIPageState::Error;
    FDivineBeastsUIViewState LoggedOut = Entry; LoggedOut.bAuthenticated = false;
    FDivineBeastsUIViewState Validating = Entry; Validating.CurrentStep = TEXT("DBA.Flow.ValidateSelection");
    FDivineBeastsUIViewState Creating = Entry; Creating.CurrentStep = TEXT("DBA.Flow.CreateCharacter");
    for (const auto& State : {Busy, Loading, Error, LoggedOut, Validating, Creating})
    {
        TestFalse(TEXT("受保护快照拒绝返回选择页"),
            FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterSelect")));
        TestFalse(TEXT("受保护快照拒绝打开创建页"),
            FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(State, TEXT("UI.Screen.CharacterCreate")));
    }
    TestEqual(TEXT("加载覆盖本地选择偏好"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(Loading, TEXT("UI.Screen.CharacterSelect")), FName(TEXT("UI.Screen.LoadingTravel")));
    Error.Loading.bIsLoading = true;
    TestEqual(TEXT("错误覆盖残留加载与创建偏好"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(Error, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.ErrorReconnect")));
    LoggedOut.CurrentStep = TEXT("DBA.Flow.Authentication");
    TestEqual(TEXT("注销后的认证节点覆盖创建偏好"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(LoggedOut, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.Login")));
    TestEqual(TEXT("权威选择验证节点覆盖创建偏好"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(Validating, TEXT("UI.Screen.CharacterCreate")), FName(TEXT("UI.Screen.CharacterSelect")));
    TestEqual(TEXT("权威创建节点不被选择偏好打断"),
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(Creating, TEXT("UI.Screen.CharacterSelect")), FName(TEXT("UI.Screen.CharacterCreate")));
    TestFalse(TEXT("角色入口不允许跳转其他业务页"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(Entry, TEXT("UI.Screen.Inventory")));
    TestFalse(TEXT("角色入口不允许直接跳转世界"),
        FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(Entry, TEXT("DBA.Flow.InWorld")));
    return true;
}

// 显示空选择页不会授予选择提交资格：调用真实VM门禁，测试快照只注入Transient反射字段。
// 测试不提交网络命令；真实角色、允许命令、忙碌和页面生命周期缺一即禁止选择。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterSelectionSubmissionGuardrailsTest,
    "DivineBeasts.UI.RoutingPolicy.SelectionSubmissionRequiresRosterAndCommand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterSelectionSubmissionGuardrailsTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsCharacterSelectViewModel> ViewModel(NewObject<UDivineBeastsCharacterSelectViewModel>());
    auto* StateProperty = FindFProperty<FStructProperty>(ViewModel->GetClass(), TEXT("State"));
    if (!TestNotNull(TEXT("真实选择VM快照字段存在"), StateProperty)) return false;
    auto* State = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(ViewModel.Get());
    State->CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    State->PageState = EDivineBeastsUIPageState::Empty;
    State->bAuthenticated = true;
    State->AllowedCommands = {TEXT("SelectPersistentCharacter")};
    ViewModel->BeginPage();
    ON_SCOPE_EXIT { ViewModel->EndPage(); };
    TestFalse(TEXT("空角色即使有选择命令也不得提交"), ViewModel->CanSubmitSelection());
    FDivineBeastsUICharacterItem Character;
    Character.CharacterId = TEXT("character-selection-fixture-001");
    State->Characters.Add(Character);
    TestTrue(TEXT("有角色且允许命令时可提交选择意图"), ViewModel->CanSubmitSelection());
    State->AllowedCommands = {TEXT("CreateCharacter")};
    TestFalse(TEXT("有角色但仅允许创建时不得提交选择"), ViewModel->CanSubmitSelection());
    State->AllowedCommands = {TEXT("SelectPersistentCharacter")};
    State->bBusy = true;
    TestFalse(TEXT("忙碌期间不得重复提交选择"), ViewModel->CanSubmitSelection());
    State->bBusy = false;
    ViewModel->EndPage();
    TestFalse(TEXT("失活页面不得提交选择"), ViewModel->CanSubmitSelection());
    return true;
}

// 真实原生创建页从快照事件刷新返回按钮；无需已有角色或选择提交命令即可取消草稿。
// 仅在Transient页面实例绑定必需控件，走CommonUI真实激活/失活；不创建资产或伪造后端。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterCreateCancelButtonTest,
    "DivineBeasts.UI.RoutingPolicy.CharacterCreateCancelButton",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterCreateCancelButtonTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsCharacterCreateScreen> Screen(NewObject<UDivineBeastsCharacterCreateScreen>());
    const TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> ViewModel(NewObject<UDivineBeastsCharacterCreateViewModel>());
    ON_SCOPE_EXIT
    {
        Screen->DeactivateWidget();
        Screen->InitializeActivatableViewModel(nullptr);
    };
    const auto BindControl = [this, Page = Screen.Get()](FName FieldName, UObject* Control)
    {
        auto* Property = FindFProperty<FObjectPropertyBase>(Page->GetClass(), FieldName);
        if (!TestNotNull(*FString::Printf(TEXT("创建页真实控件字段存在：%s"), *FieldName.ToString()), Property)) return false;
        Property->SetObjectPropertyValue_InContainer(Page, Control);
        return true;
    };
    auto* BackButton = NewObject<UButton>(Screen.Get());
    auto* ErrorText = NewObject<UTextBlock>(Screen.Get());
    if (!BindControl(TEXT("BackToCharacterSelectButton"), BackButton) ||
        !BindControl(TEXT("CreateButton"), NewObject<UButton>(Screen.Get())) ||
        !BindControl(TEXT("LogoutButton"), NewObject<UButton>(Screen.Get())) ||
        !BindControl(TEXT("HeroOptions"), NewObject<UComboBoxString>(Screen.Get())) ||
        !BindControl(TEXT("CharacterNameInput"), NewObject<UEditableTextBox>(Screen.Get())) ||
        !BindControl(TEXT("StatusText"), NewObject<UTextBlock>(Screen.Get())) ||
        !BindControl(TEXT("ErrorText"), ErrorText)) return false;
    auto* StateProperty = FindFProperty<FStructProperty>(ViewModel->GetClass(), TEXT("State"));
    if (!TestNotNull(TEXT("真实创建VM快照字段存在"), StateProperty)) return false;
    auto* State = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(ViewModel.Get());
    FDivineBeastsUIViewState Entry;
    Entry.CurrentStep = TEXT("DBA.Flow.CharacterEntry");
    Entry.PageState = EDivineBeastsUIPageState::Empty;
    Entry.bAuthenticated = true;
    Entry.AllowedCommands = {TEXT("CreateCharacter")};
    *State = Entry;
    Screen->InitializeActivatableViewModel(ViewModel.Get());
    Screen->ActivateWidget();
    TestTrue(TEXT("空角色创建页可返回选择页"), BackButton->GetIsEnabled());
    State->AllowedCommands.Reset();
    ViewModel->MarkStateChanged();
    TestTrue(TEXT("取消按钮不借用业务选择提交权限"), BackButton->GetIsEnabled());

    // 各拒绝条件单独刷新，确保放开空角色取消不放宽其他状态门禁。
    for (int32 GuardIndex = 0; GuardIndex < 5; ++GuardIndex)
    {
        *State = Entry;
        if (GuardIndex == 0) State->bBusy = true;
        if (GuardIndex == 1) State->Loading.bIsLoading = true;
        if (GuardIndex == 2)
        {
            State->PageState = EDivineBeastsUIPageState::Error;
            State->ErrorText = FText::FromString(TEXT("创建失败仍由权威快照报告"));
        }
        if (GuardIndex == 3) State->bAuthenticated = false;
        if (GuardIndex == 4) State->CurrentStep = TEXT("DBA.Flow.ValidateSelection");
        ViewModel->MarkStateChanged();
        TestFalse(*FString::Printf(TEXT("返回按钮遵守状态门禁%d"), GuardIndex), BackButton->GetIsEnabled());
        if (GuardIndex == 2) TestTrue(TEXT("取消资格刷新不吞掉原错误"), ErrorText->GetText().EqualTo(State->ErrorText));
    }
    *State = Entry;
    ViewModel->MarkStateChanged();
    TestTrue(TEXT("恢复空闲角色入口后事件重新启用取消"), BackButton->GetIsEnabled());
    return true;
}

namespace
{
/**
 * 角色选择页原生回归夹具：创建Transient页面、真实命名控件树和VM，复用生产激活、状态及焦点入口。
 * 手工快照仅为UI输入，不取得后端授权；不写资产、不创建世界，不以模拟函数返回值代替页面行为。
 * 析构结束激活和VM订阅；控件由WidgetTree及页面反射字段持有，生命周期限于本条自动化测试。
 */
struct FDivineBeastsCharacterSelectNativeFixture
{
    TStrongObjectPtr<UDivineBeastsCharacterSelectScreen> Screen;
    TStrongObjectPtr<UDivineBeastsCharacterSelectViewModel> ViewModel;
    UButton* CreateButton = nullptr;
    UButton* SelectButton = nullptr;
    UTextBlock* SelectButtonLabel = nullptr;
    UComboBoxString* CharacterList = nullptr;
    UTextBlock* StatusText = nullptr;
    FDivineBeastsUIViewState* State = nullptr;

    /** 在游戏线程绑定真实字段并激活空角色入口；可聚焦参数仅控制测试按钮前提，false用于负向用例。 */
    bool Initialize(FAutomationTestBase& Test, bool bCreateButtonFocusable = true)
    {
        Screen.Reset(NewObject<UDivineBeastsCharacterSelectScreen>());
        ViewModel.Reset(NewObject<UDivineBeastsCharacterSelectViewModel>());
        Screen->WidgetTree = NewObject<UWidgetTree>(Screen.Get());
        auto* Root = Screen->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RegressionRoot"));
        Screen->WidgetTree->RootWidget = Root;
        CharacterList = Screen->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("CharacterList"));
        CreateButton = Screen->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("OpenCharacterCreateButton"));
        // UE5.8的UButton没有公开SetIsFocusable，InitIsFocusable受保护；仅为Transient夹具设置真实反射配置。
        // 必须在TakeWidget前明确焦点属性；IsVisible查询缓存Slate，只有UMG控件树会误报“不可见”。
        auto* FocusableProperty = FindFProperty<FBoolProperty>(CreateButton->GetClass(), TEXT("IsFocusable"));
        if (!Test.TestNotNull(TEXT("真实按钮可聚焦配置字段存在"), FocusableProperty)) return false;
        FocusableProperty->SetPropertyValue_InContainer(CreateButton, bCreateButtonFocusable);
        CreateButton->TakeWidget();
        if (!Test.TestTrue(TEXT("创建按钮已构建真实Slate控件"), CreateButton->GetCachedWidget().IsValid())) return false;
        SelectButton = Screen->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
        SelectButtonLabel = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectButtonLabel"));
        SelectButtonLabel->SetText(FText::FromString(TEXT("进入游戏")));
        SelectButton->AddChild(SelectButtonLabel);
        // RED阶段允许旧页面缺少新增可选绑定；测试仍运行真实按钮投影，不能因字段缺失直接中止。
        if (auto* LabelProperty = FindFProperty<FObjectPropertyBase>(Screen->GetClass(), TEXT("SelectButtonLabel")))
            LabelProperty->SetObjectPropertyValue_InContainer(Screen.Get(), SelectButtonLabel);
        auto* LogoutButton = Screen->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LogoutButton"));
        StatusText = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
        auto* ErrorText = Screen->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ErrorText"));
        const auto BindControl = [&Test, Page = Screen.Get(), Root](FName Name, UWidget* Control)
        {
            auto* Property = FindFProperty<FObjectPropertyBase>(Page->GetClass(), Name);
            if (!Test.TestNotNull(*FString::Printf(TEXT("角色选择页真实字段存在：%s"), *Name.ToString()), Property)) return false;
            Property->SetObjectPropertyValue_InContainer(Page, Control);
            Root->AddChildToVerticalBox(Control);
            return true;
        };
        if (!BindControl(TEXT("CharacterList"), CharacterList) ||
            !BindControl(TEXT("OpenCharacterCreateButton"), CreateButton) ||
            !BindControl(TEXT("SelectButton"), SelectButton) ||
            !BindControl(TEXT("LogoutButton"), LogoutButton) ||
            !BindControl(TEXT("StatusText"), StatusText) ||
            !BindControl(TEXT("ErrorText"), ErrorText)) return false;
        auto* StateProperty = FindFProperty<FStructProperty>(ViewModel->GetClass(), TEXT("State"));
        if (!Test.TestNotNull(TEXT("真实选择VM快照字段存在"), StateProperty)) return false;
        State = StateProperty->ContainerPtrToValuePtr<FDivineBeastsUIViewState>(ViewModel.Get());
        State->CurrentStep = TEXT("DBA.Flow.CharacterEntry");
        State->PageState = EDivineBeastsUIPageState::Empty;
        State->bAuthenticated = true;
        State->AllowedCommands = {TEXT("CreateCharacter")};
        Screen->InitializeScreen(TEXT("UI.Screen.CharacterSelect"), ViewModel.Get(), TEXT("CharacterList"),
            EGamePlatformUIInputMode::UIOnly, EGamePlatformUIPausePolicy::Never);
        Screen->ActivateWidget();
        return Test.TestTrue(TEXT("真实选择页已激活"), Screen->IsActivated()) &&
            Test.TestTrue(TEXT("真实选择VM页面生命周期已开始"), ViewModel->IsPageActive());
    }

    ~FDivineBeastsCharacterSelectNativeFixture()
    {
        if (Screen.IsValid())
        {
            if (Screen->IsActivated()) Screen->DeactivateWidget();
            Screen->InitializeActivatableViewModel(nullptr);
        }
    }
};
}

// 空列表提示必须引导显式创建；恢复档案、忙碌和禁用原因仍从真实状态事件投影，不能被空态文案覆盖。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterSelectEmptyRosterPresentationTest,
    "DivineBeasts.UI.Characters.CharacterSelect.EmptyRosterPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterSelectEmptyRosterPresentationTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsCharacterSelectNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    const FText EmptyPrompt = FText::FromString(TEXT("尚未创建角色，请点击创建角色开始。"));
    TestTrue(TEXT("空角色入口提示创建而非确认选择"), Fixture.StatusText->GetText().EqualTo(EmptyPrompt));
    TestFalse(TEXT("空列表提示不会授予选择提交资格"), Fixture.ViewModel->CanSubmitSelection());
    Fixture.State->bBusy = true;
    Fixture.ViewModel->MarkStateChanged();
    TestTrue(TEXT("忙碌快照仍显示原有提交提示"),
        Fixture.StatusText->GetText().EqualTo(FText::FromString(TEXT("正在确认角色，请稍候…"))));
    Fixture.State->bBusy = false;
    FDivineBeastsUICharacterItem Character;
    Character.CharacterId = TEXT("character-empty-state-fixture-001");
    Character.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Character.DisplayName = FText::FromString(TEXT("回归档案"));
    Fixture.State->Characters.Add(Character);
    Fixture.State->PageState = EDivineBeastsUIPageState::Ready;
    Fixture.ViewModel->MarkStateChanged();
    TestTrue(TEXT("非空档案保留原有确认提示"),
        Fixture.StatusText->GetText().EqualTo(FText::FromString(TEXT("选择角色后点击确认。"))));
    Fixture.State->Characters[0].bEnabled = false;
    Fixture.State->Characters[0].DisabledReason = FText::FromString(TEXT("档案当前不可用"));
    Fixture.ViewModel->MarkStateChanged();
    TestTrue(TEXT("非空禁用档案保留真实禁用原因"),
        Fixture.StatusText->GetText().EqualTo(Fixture.State->Characters[0].DisabledReason));
    Fixture.State->Characters.Reset();
    Fixture.State->PageState = EDivineBeastsUIPageState::Empty;
    Fixture.ViewModel->MarkStateChanged();
    TestTrue(TEXT("档案移除后的事件重新显示创建提示"), Fixture.StatusText->GetText().EqualTo(EmptyPrompt));
    return true;
}

// 空列表应把焦点交给可用创建按钮；通过CommonUI公开焦点入口读取生产Native路由，不手工调用保护函数。
// 下拉框折叠模拟现代页面的非交互旧控件；禁用、忙碌或隐藏的创建按钮不得成为优先目标。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterSelectEmptyRosterFocusTest,
    "DivineBeasts.UI.Characters.CharacterSelect.EmptyRosterCreateFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterSelectEmptyRosterFocusTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsCharacterSelectNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    Fixture.CharacterList->SetVisibility(ESlateVisibility::Collapsed);
    if (!TestTrue(TEXT("创建入口已获快照授权且启用"), Fixture.CreateButton->GetIsEnabled()) ||
        !TestTrue(TEXT("正向创建入口明确允许键盘焦点"), Fixture.CreateButton->GetIsFocusable()) ||
        !TestTrue(TEXT("正向创建入口的真实Slate可见"), Fixture.CreateButton->IsVisible())) return false;
    TestEqual(TEXT("空列表优先聚焦可用创建入口"), Fixture.Screen->GetDesiredFocusTarget(), static_cast<UWidget*>(Fixture.CreateButton));
    Fixture.State->AllowedCommands.Reset();
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("创建权限撤销后不再优先聚焦禁用按钮"), Fixture.Screen->GetDesiredFocusTarget() == Fixture.CreateButton);
    Fixture.State->AllowedCommands = {TEXT("CreateCharacter")};
    Fixture.State->bBusy = true;
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("忙碌时不再优先聚焦创建按钮"), Fixture.Screen->GetDesiredFocusTarget() == Fixture.CreateButton);
    Fixture.State->bBusy = false;
    Fixture.ViewModel->MarkStateChanged();
    Fixture.CreateButton->SetVisibility(ESlateVisibility::Collapsed);
    TestFalse(TEXT("隐藏创建入口不得作为优先焦点"), Fixture.Screen->GetDesiredFocusTarget() == Fixture.CreateButton);
    Fixture.CreateButton->SetVisibility(ESlateVisibility::Visible);
    TestEqual(TEXT("恢复可用创建入口后焦点路由恢复"), Fixture.Screen->GetDesiredFocusTarget(), static_cast<UWidget*>(Fixture.CreateButton));
    // 同一生产门禁必须拒绝“可见且启用但不允许键盘焦点”的入口，不能靠放宽门禁使正向用例通过。
    FDivineBeastsCharacterSelectNativeFixture NonFocusableFixture;
    if (!NonFocusableFixture.Initialize(*this, false)) return false;
    TestTrue(TEXT("非聚焦反例仍有可见且启用的创建按钮"),
        NonFocusableFixture.CreateButton->IsVisible() && NonFocusableFixture.CreateButton->GetIsEnabled());
    TestFalse(TEXT("非聚焦反例明确不允许键盘焦点"), NonFocusableFixture.CreateButton->GetIsFocusable());
    TestFalse(TEXT("不可聚焦按钮不得成为优先焦点"),
        NonFocusableFixture.Screen->GetDesiredFocusTarget() == NonFocusableFixture.CreateButton);
    return true;
}

// 非空档案仍优先当前卡片按钮，创建入口不能抢占；手工装配真实条目按钮以隔离资产和世界环境。
// 反射只设置测试页面的Transient条目集合，选项切换、事件刷新及焦点查询仍全部使用生产入口。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterSelectNonEmptyRosterFocusTest,
    "DivineBeasts.UI.Characters.CharacterSelect.NonEmptyRosterCardFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterSelectNonEmptyRosterFocusTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsCharacterSelectNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FDivineBeastsUICharacterItem Character;
        Character.CharacterId = FString::Printf(TEXT("character-focus-fixture-%d"), Index);
        Character.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
        Character.DisplayName = FText::FromString(FString::Printf(TEXT("焦点档案%d"), Index));
        Fixture.State->Characters.Add(Character);
    }
    Fixture.State->PageState = EDivineBeastsUIPageState::Ready;
    Fixture.ViewModel->MarkStateChanged();
    auto* ChoicesProperty = FindFProperty<FArrayProperty>(Fixture.Screen->GetClass(), TEXT("ChoiceEntries"));
    auto* ButtonProperty = FindFProperty<FObjectPropertyBase>(UDivineBeastsCharacterChoiceEntry::StaticClass(), TEXT("ChoiceButton"));
    if (!TestNotNull(TEXT("真实页面Transient卡片集合存在"), ChoicesProperty) ||
        !TestNotNull(TEXT("真实卡片焦点按钮字段存在"), ButtonProperty)) return false;
    auto* Choices = ChoicesProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UDivineBeastsCharacterChoiceEntry>>>(Fixture.Screen.Get());
    UButton* CardButtons[2] = {};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Entry = NewObject<UDivineBeastsCharacterChoiceEntry>(Fixture.Screen.Get());
        CardButtons[Index] = NewObject<UButton>(Entry);
        ButtonProperty->SetObjectPropertyValue_InContainer(Entry, CardButtons[Index]);
        Entry->ConfigureChoice(Fixture.State->Characters[Index].CharacterId, {}, {}, {});
        Choices->Add(Entry);
    }
    TestEqual(TEXT("第一个档案仍优先卡片按钮"), Fixture.Screen->GetDesiredFocusTarget(), static_cast<UWidget*>(CardButtons[0]));
    Fixture.CharacterList->SetSelectedIndex(1);
    TestEqual(TEXT("选择变化后焦点仍跟随对应档案卡片"), Fixture.Screen->GetDesiredFocusTarget(), static_cast<UWidget*>(CardButtons[1]));
    TestFalse(TEXT("非空档案的创建入口不抢占卡片焦点"), Fixture.Screen->GetDesiredFocusTarget() == Fixture.CreateButton);
    return true;
}

// 主入口不能对无档案的新玩家表现为失效按钮；仅引导创建，真实角色选择资格仍为false。
// 有角色、忙碌、未认证和缺命令分别使用真实快照事件，防止以UI可点击偷渡业务授权。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterEntryPrimaryActionTest,
    "DivineBeasts.UI.Characters.CharacterSelect.PrimaryActionFeedback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterEntryPrimaryActionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    FDivineBeastsCharacterSelectNativeFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    TestTrue(TEXT("空档案主按钮可引导创建而非静默禁用"), Fixture.SelectButton->GetIsEnabled());
    TestTrue(TEXT("空档案主按钮明确表达创建而非进入已有角色"),
        Fixture.SelectButtonLabel->GetText().EqualTo(FText::FromString(TEXT("创建角色"))));
    TestFalse(TEXT("可打开创建页不等于可提交角色选择"), Fixture.ViewModel->CanSubmitSelection());
    Fixture.State->bBusy = true;
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("忙碌期间不能重复导航或进入"), Fixture.SelectButton->GetIsEnabled());
    Fixture.State->bBusy = false;
    Fixture.State->bAuthenticated = false;
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("未认证不得通过创建引导绕过入口"), Fixture.SelectButton->GetIsEnabled());
    Fixture.State->bAuthenticated = true;
    Fixture.State->AllowedCommands.Reset();
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("没有创建能力时主按钮仍禁止执行"), Fixture.SelectButton->GetIsEnabled());
    Fixture.State->AllowedCommands = {TEXT("SelectPersistentCharacter")};
    Fixture.State->PageState = EDivineBeastsUIPageState::Ready;
    FDivineBeastsUICharacterItem Character;
    Character.CharacterId = TEXT("primary-action-real-roster-fixture");
    Character.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Character.bEnabled = true;
    Fixture.State->Characters.Add(Character);
    Fixture.ViewModel->MarkStateChanged();
    TestTrue(TEXT("可用档案与后端选择能力才启用进入游戏"), Fixture.SelectButton->GetIsEnabled());
    TestTrue(TEXT("已有档案主按钮恢复进入游戏语义"),
        Fixture.SelectButtonLabel->GetText().EqualTo(FText::FromString(TEXT("进入游戏"))));
    Fixture.State->Characters[0].bEnabled = false;
    Fixture.ViewModel->MarkStateChanged();
    TestFalse(TEXT("不可用档案不能提交进入"), Fixture.SelectButton->GetIsEnabled());
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
