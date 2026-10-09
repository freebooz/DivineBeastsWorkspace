#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsArenaUIScreenCatalog.h"
#include "UI/HUD/DivineBeastsArenaHUD.h"
#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "UI/GamePlatformMobaArenaModalScreenBase.h"
#include "UI/GamePlatformMobaArenaScreenBase.h"
#include "UI/Screens/DivineBeastsArenaHeroSelectionScreen.h"
#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "UI/Screens/DivineBeastsMatchFoundReadyScreen.h"
#include "UI/Screens/DivineBeastsMatchmakingScreen.h"
#include "UI/Screens/DivineBeastsPostMatchResultScreen.h"
#include "UI/Screens/DivineBeastsScoreboardScreen.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Screens/GamePlatformUIScreen.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Misc/ScopeExit.h"

/**
 * 仅WITH_DEV_AUTOMATION_TESTS：受控fixture登记真实具体实例/原栈，为真实Manager关闭路径建立前提。
 * 不创建Data成功租约、不模拟OpenScreenAsync成功；职责只覆盖已登记页面撤账与Closed通知次序。
 * 中立名称对应平台测试friend，不让平台公开头认识DivineBeasts类型或生产写接口。
 */
struct FGamePlatformUIScreenOwnershipTestAccess
{
    static void Register(UGamePlatformUIManagerSubsystem& Manager, UGamePlatformUIScreen& Screen,
        UCommonActivatableWidgetStack& Stack)
    {
        Manager.ScreenStacks.Add(&Screen, &Stack);
    }
};

/**
 * 验证竞技UI表面只归DBAArena所有，并保持项目层 → MOBA通用层 → 平台层的单向继承。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaUISurfaceInventoryTest,
    "DivineBeasts.Arena.UI.SurfaceInventory",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaUISurfaceInventoryTest::RunTest(const FString&)
{
    const TArray<FDivineBeastsArenaUISurfaceDescriptor>& Surfaces =
        FDivineBeastsArenaUIScreenCatalog::GetSurfaces();

    TestEqual(TEXT("DBAArena应拥有5个Screen加1个ArenaHUD"), Surfaces.Num(), 6);

    TSet<FName> UniqueIds;
    int32 ScreenCount = 0;
    int32 HUDCount = 0;
    for (const FDivineBeastsArenaUISurfaceDescriptor& Surface : Surfaces)
    {
        TestTrue(TEXT("竞技SurfaceId必须有效"), !Surface.SurfaceId.IsNone());
        TestFalse(TEXT("竞技SurfaceId必须唯一"), UniqueIds.Contains(Surface.SurfaceId));
        UniqueIds.Add(Surface.SurfaceId);

        TestTrue(
            TEXT("竞技软资源路径必须归DBAArena挂载点"),
            Surface.WidgetClassPath.StartsWith(TEXT("/DBAArena/")));
        // 未完成蓝图生成及移动端Cook前，不得登记无法解析的移动端变体。
        TestTrue(
            TEXT("竞技移动端资源未交付时必须回退公共Widget"),
            Surface.MobileWidgetClassPath.IsEmpty());

        if (Surface.Kind == EDivineBeastsArenaUISurfaceKind::Screen)
        {
            ++ScreenCount;
        }
        else if (Surface.Kind == EDivineBeastsArenaUISurfaceKind::HUD)
        {
            ++HUDCount;
        }
    }

    TestEqual(TEXT("竞技Screen数量"), ScreenCount, 5);
    TestEqual(TEXT("竞技HUD数量"), HUDCount, 1);

    for (const FName RequiredId : {
         FName(TEXT("UI.Screen.Matchmaking")),
         FName(TEXT("UI.Screen.MatchFoundReady")),
         FName(TEXT("UI.Screen.ArenaHeroSelection")),
         FName(TEXT("UI.Screen.Scoreboard")),
         FName(TEXT("UI.Screen.PostMatchResult")),
         FName(TEXT("UI.HUD.Arena"))})
    {
        TestNotNull(
            *FString::Printf(
                TEXT("DBAArena必须拥有表面 %s"),
                *RequiredId.ToString()),
            FDivineBeastsArenaUIScreenCatalog::Find(RequiredId));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaUIHierarchyTest,
    "DivineBeasts.Arena.UI.Hierarchy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaUIHierarchyTest::RunTest(const FString&)
{
    TestTrue(
        TEXT("项目ArenaHUD必须继承MOBA通用ArenaHUD"),
        UDivineBeastsArenaHUD::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaHUDBase::StaticClass()));

    TestTrue(
        TEXT("项目Arena普通页面基类必须继承MOBA通用ArenaScreen"),
        UDivineBeastsArenaScreenBase::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaScreenBase::StaticClass()));

    TestTrue(
        TEXT("匹配成功准备页必须使用MOBA竞技Modal基类"),
        UDivineBeastsMatchFoundReadyScreen::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaModalScreenBase::StaticClass()));

    TestTrue(
        TEXT("四个普通竞技页面必须继承项目ArenaScreen基类"),
        UDivineBeastsMatchmakingScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsArenaHeroSelectionScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsScoreboardScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsPostMatchResultScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()));

    return true;
}

/**
 * 真实CommonUI同内容ID的两个Native实例：Scope只允许测试分配抽象类，不改ClassFlags/资产。
 * 受控fixture只登记ScreenStacks，不伪造Data租约；真实Manager.CloseScreen/Deinitialize发布Closed。
 * 同ID外部关闭与替换旧页不清后继；Root撤账时旧WidgetList尚有自有A也必须清除Active。
 * 未覆盖完整Data异步Open或生产Root创建/视觉，不能将测试账本注入当作生产加载受理。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaUIScreenInstanceOwnershipRegressionTest,
    "DivineBeasts.Arena.UI.SameIdInstanceOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaUIScreenInstanceOwnershipRegressionTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实Engine Within宿主"), GEngine)) return false;
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Manager(NewObject<UGamePlatformUIManagerSubsystem>(Player.Get()));
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> Service(NewObject<UDivineBeastsArenaUIClientSubsystem>(Player.Get()));
    const TStrongObjectPtr<UGamePlatformUILayerStack> Root(NewObject<UGamePlatformUILayerStack>());
    const TStrongObjectPtr<UCommonActivatableWidgetStack> Stack(NewObject<UCommonActivatableWidgetStack>());
    const TStrongObjectPtr<UGamePlatformUIScreen> A(NewObject<UGamePlatformUIScreen>());
    const TStrongObjectPtr<UGamePlatformUIScreen> B(NewObject<UGamePlatformUIScreen>());
    auto* RootProperty = FindFProperty<FObjectPropertyBase>(Manager->GetClass(), TEXT("RootLayout"));
    auto* LayerProperty = FindFProperty<FObjectPropertyBase>(Root->GetClass(), TEXT("ScreenLayer"));
    if (!TestNotNull(TEXT("真实RootLayout字段"), RootProperty) ||
        !TestNotNull(TEXT("真实ScreenLayer字段"), LayerProperty)) return false;
    RootProperty->SetObjectPropertyValue_InContainer(Manager.Get(), Root.Get());
    LayerProperty->SetObjectPropertyValue_InContainer(Root.Get(), Stack.Get());
    Service->PlatformUI = Manager.Get();
    Manager->OnScreenClosed.AddDynamic(Service.Get(), &UDivineBeastsArenaUIClientSubsystem::HandleArenaScreenClosed);
    Service->ArenaViewModel = NewObject<UGamePlatformArenaViewModel>(Service.Get());
    Service->ArenaViewModel->SetObservedClientFlowState(EGamePlatformArenaClientFlowState::Matchmaking);
    const FName Id(TEXT("UI.Screen.Matchmaking"));
    A->InitializeScreen(Id, nullptr, NAME_None, EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    B->InitializeScreen(Id, nullptr, NAME_None, EGamePlatformUIInputMode::GameAndUI, EGamePlatformUIPausePolicy::Never);
    Stack->SetTransitionDuration(0.0f);
    const TSharedRef<SWidget> SlateStack = Stack->TakeWidget();
    ON_SCOPE_EXIT
    {
        Service->Deinitialize();
        Stack->ClearWidgets();
        Manager->Deinitialize();
    };
    Stack->AddWidgetInstance(*A);
    FGamePlatformUIScreenOwnershipTestAccess::Register(*Manager, *A, *Stack);
    const FGuid FirstRequest = FGuid::NewGuid();
    Service->OpeningArenaRequestId = FirstRequest;
    Service->HandleArenaScreenOpened(FirstRequest, Id, A.Get());
    if (!TestEqual(TEXT("具体A登记为自有页面"), Service->ActiveArenaScreen.Get(), A.Get())) return false;
    Stack->AddWidgetInstance(*B);
    FGamePlatformUIScreenOwnershipTestAccess::Register(*Manager, *B, *Stack);
    TestTrue(TEXT("同ID两个不同实例同时属于真实原栈"),
        A.Get() != B.Get() && Stack->GetWidgetList().Contains(A.Get()) && Stack->GetWidgetList().Contains(B.Get()));
    TestFalse(TEXT("被B覆盖的A允许暂失活"), A->IsActivated());
    TestTrue(TEXT("真实Manager关闭外部同ID B"), Manager->CloseScreen(B.Get()));
    if (!TestFalse(TEXT("外部B真实离开CommonUI"), Stack->GetWidgetList().Contains(B.Get()))) return false;
    TestEqual(TEXT("关闭外部同ID B不遗忘仍在栈的A"), Service->ActiveArenaScreen.Get(), A.Get());
    TestEqual(TEXT("拥有仍指登记时原栈"), Service->ActiveArenaScreenStack.Get(), Stack.Get());

    Stack->AddWidgetInstance(*B);
    FGamePlatformUIScreenOwnershipTestAccess::Register(*Manager, *B, *Stack);
    const FGuid ReplacementRequest = FGuid::NewGuid();
    Service->OpeningArenaRequestId = ReplacementRequest;
    Service->HandleArenaScreenOpened(ReplacementRequest, Id, B.Get());
    TestEqual(TEXT("新同ID B成为具体自有实例"), Service->ActiveArenaScreen.Get(), B.Get());
    // HandleOpened替换旧A实际调用Manager.CloseScreen并发同ID Closed；B的精确账本仍属原栈。
    if (!TestFalse(TEXT("替换的旧A真实离开原栈"), Stack->GetWidgetList().Contains(A.Get()))) return false;
    TestEqual(TEXT("旧同ID A的关闭不误清新B"), Service->ActiveArenaScreen.Get(), B.Get());
    TestTrue(TEXT("真实Manager关闭自有B"), Manager->CloseScreen(B.Get()));
    if (!TestFalse(TEXT("自有B真实离开原栈"), Stack->GetWidgetList().Contains(B.Get()))) return false;
    TestNull(TEXT("自有具体实例真正离场才清账"), Service->ActiveArenaScreen.Get());
    TestFalse(TEXT("离场释放原栈身份"), Service->ActiveArenaScreenStack.IsValid());

    // 真实Deinitialize与Root替换共用ClearScreenOwnership：先撤实例Key，再Closed，最后拆Root。
    // Root是受控Native夹具且未安装到Viewport；原CommonUI栈保持活着，精确复现旧成员仍在场。
    Stack->AddWidgetInstance(*A);
    FGamePlatformUIScreenOwnershipTestAccess::Register(*Manager, *A, *Stack);
    const FGuid RootExitRequest = FGuid::NewGuid();
    Service->OpeningArenaRequestId = RootExitRequest;
    Service->HandleArenaScreenOpened(RootExitRequest, Id, A.Get());
    if (!TestEqual(TEXT("撤Root前拥有具体A"), Service->ActiveArenaScreen.Get(), A.Get())) return false;
    TestTrue(TEXT("撤Root前平台具体账本有效"), Manager->IsScreenOwnedByStack(A.Get(), Stack.Get()));
    Manager->Deinitialize(); // 真实清账与真实动态Closed通知，不手工广播/调用Closed处理器。
    TestTrue(TEXT("根撤账期间旧CommonUI成员仍存活"), Stack->GetWidgetList().Contains(A.Get()));
    TestFalse(TEXT("已关闭Manager不能报告旧实例归属"), Manager->IsScreenOwnedByStack(A.Get(), Stack.Get()));
    TestNull(TEXT("真实Root撤账Closed立即清具体A而不等待GC"), Service->ActiveArenaScreen.Get());
    TestFalse(TEXT("真实Root撤账释放原栈身份"), Service->ActiveArenaScreenStack.IsValid());
    (void)SlateStack;
    return true;
}
#endif
