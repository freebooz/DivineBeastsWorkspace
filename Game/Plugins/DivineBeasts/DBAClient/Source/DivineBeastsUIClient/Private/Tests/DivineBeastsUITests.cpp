#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"
#include "Screens/Connection/DivineBeastsErrorReconnectScreen.h"

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
