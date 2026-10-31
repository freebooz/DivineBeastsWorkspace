// 客户端命令桥安全回归：没有认证/完整生命周期能力时不产生报名或候选确认副作用。
// 仅Editor自动化测试使用计数替身，不连接后端，不进入生产组合根。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Client/GamePlatformArenaMatchmakingAdapter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UI/Screens/DivineBeastsMatchmakingScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ListView.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBox.h"
#include "Widgets/Social/DivineBeastsPartyListEntry.h"
#include "ViewModels/GamePlatformArenaViewModel.h"

namespace
{
    class FIncompleteMatchmakingTestAdapter final : public IGamePlatformArenaMatchmakingAdapter
    {
    public:
        int32 Mutations = 0;
        EGamePlatformArenaClientFlowState ObservedState=EGamePlatformArenaClientFlowState::Idle;
        virtual void RequestMatchmaking(const FGamePlatformArenaMatchmakingRequest&,TFunction<void(bool,const FString&)> Done) override
        {++Mutations;Done(false,TEXT("Test.UnexpectedMutation"));}
        virtual void CancelMatchmaking(const FString&,TFunction<void(bool,const FString&)> Done) override
        {++Mutations;Done(false,TEXT("Test.UnexpectedMutation"));}
        virtual EGamePlatformArenaClientFlowState GetObservedState() const override {return ObservedState;}
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaMatchmakingCommandGateTest,
    "DivineBeasts.Arena.UI.MatchmakingCommandGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaMatchmakingCommandGateTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> Service(NewObject<UDivineBeastsArenaUIClientSubsystem>(Player.Get()));
    const auto Adapter=MakeShared<FIncompleteMatchmakingTestAdapter>();
    const FGuid Registration=Service->RegisterMatchmakingAdapter(Adapter);
    TestTrue(TEXT("同玩家首次提供者获得精确注册身份"),Registration.IsValid());
    TestFalse(TEXT("不能覆盖已有提供者"),Service->RegisterMatchmakingAdapter(Adapter).IsValid());
    TestFalse(TEXT("创建接口不等于完整生命周期"),Service->CanRequestMatchmaking());
    TestFalse(TEXT("没有认证/正式模式时不报名"),Service->RequestMatchmaking(TEXT("Arena.Mode.Duel1v1")));
    TestFalse(TEXT("没有真实票据时不取消"),Service->CancelMatchmaking());
    TestFalse(TEXT("没有候选身份不确认"),Service->RespondToMatchOffer(true));
    TestFalse(TEXT("没有候选身份不拒绝"),Service->RespondToMatchOffer(false));
    TestFalse(TEXT("没有认证不能发起固定对手邀请"),Service->SendChallenge(TEXT("Arena.Mode.Duel1v1"),TEXT("public-opponent")));
    TestFalse(TEXT("没有邀请身份不能接受"),Service->ReplyChallenge(true));
    TestFalse(TEXT("没有邀请身份不能拒绝"),Service->ReplyChallenge(false));
    TestFalse(TEXT("没有邀请身份不能取消"),Service->CancelChallengeInvitation());
    TestEqual(TEXT("全部非法命令未调用远端副作用"),Adapter->Mutations,0);
    Service->UnregisterMatchmakingAdapter(FGuid::NewGuid());
    TestFalse(TEXT("错误句柄不能撤销原提供者"),Service->RegisterMatchmakingAdapter(Adapter).IsValid());
    Service->UnregisterMatchmakingAdapter(Registration);
    TestTrue(TEXT("精确撤销后可重新注册"),Service->RegisterMatchmakingAdapter(Adapter).IsValid());
    return true;
}

/** 只检验客户端已确认终态的身份清理策略；不模拟生产报名、签票或隐藏分。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaConfirmedOfferRegressionTest,
    "DivineBeasts.Arena.UI.ConfirmedOfferTerminal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaConfirmedOfferRegressionTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> Service(NewObject<UDivineBeastsArenaUIClientSubsystem>(Player.Get()));
    Service->MatchmakingLogicalRequestId=TEXT("current-ticket-request");
    Service->CompleteConfirmedTicketState(TEXT("old-ticket-request"),EGamePlatformArenaClientFlowState::Idle);
    TestFalse(TEXT("旧身份不能清新报名"),Service->MatchmakingLogicalRequestId.IsEmpty());
    Service->CompleteConfirmedTicketState(TEXT("current-ticket-request"),EGamePlatformArenaClientFlowState::Failed);
    TestFalse(TEXT("失败不等于确定释放远端名单"),Service->MatchmakingLogicalRequestId.IsEmpty());
    Service->CompleteConfirmedTicketState(TEXT("current-ticket-request"),EGamePlatformArenaClientFlowState::Idle);
    TestTrue(TEXT("服务器确认候选终结且Idle后解除本地再次报名门禁"),Service->MatchmakingLogicalRequestId.IsEmpty());
    return true;
}

/** 实际原生选择委托回归，不创建/修改蓝图，不把未配置模式变成可报名模式。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaModeSelectionRegressionTest,
    "DivineBeasts.Arena.UI.KeyboardModeSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaModeSelectionRegressionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsMatchmakingScreen> Page(NewObject<UDivineBeastsMatchmakingScreen>());
    Page->WidgetTree=NewObject<UWidgetTree>(Page.Get());
    auto* List=Page->WidgetTree->ConstructWidget<UListView>(UListView::StaticClass(),TEXT("ModeList"));
    Page->WidgetTree->RootWidget=List;
    Page->BindUIEvents();
    auto* Duel=NewObject<UDivineBeastsPartyListItem>(Page.Get());Duel->Key=TEXT("Arena.Mode.Duel1v1");
    auto* Team=NewObject<UDivineBeastsPartyListItem>(Page.Get());Team->Key=TEXT("Arena.Mode.Team5v5");
    List->OnItemSelectionChanged().Broadcast(Duel);
    TestEqual(TEXT("键盘选择1v1同步真实模式身份"),Page->SelectedMode,FName(*Duel->Key));
    List->OnItemSelectionChanged().Broadcast(Team);
    TestEqual(TEXT("后继选择5v5不保留旧模式"),Page->SelectedMode,FName(*Team->Key));
    List->OnItemSelectionChanged().Broadcast(nullptr);
    TestTrue(TEXT("取消选择后不能报名旧模式"),Page->SelectedMode.IsNone());
    Page->UnbindUIEvents();
    TestFalse(TEXT("失活解除选择观察"),List->OnItemSelectionChanged().IsBoundToObject(Page.Get()));
    return true;
}

/** 第二个门或未知写入不能改写已提交模式；浏览模式只接受目录中的稳定身份。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaPortalSelectionRegressionTest,
    "DivineBeasts.Arena.UI.PortalModePreselection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaPortalSelectionRegressionTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> Service(NewObject<UDivineBeastsArenaUIClientSubsystem>(Player.Get()));
    TestTrue(TEXT("3v3门预选目录模式"),Service->SelectMatchmakingMenuMode(TEXT("Arena.Mode.Team3v3")));
    TestEqual(TEXT("面板读取门的预选身份"),Service->GetSelectedMatchmakingMode(),FName(TEXT("Arena.Mode.Team3v3")));
    TestFalse(TEXT("非法入口身份不改变当前模式"),Service->SelectMatchmakingMenuMode(TEXT("Arena.Mode.Nonexistent")));
    Service->MatchmakingLogicalRequestId=TEXT("unconfirmed-start");
    TestFalse(TEXT("结果未知时第二个门不能改成1v1"),Service->SelectMatchmakingMenuMode(TEXT("Arena.Mode.Duel1v1")));
    TestEqual(TEXT("未知结果保留原已提交模式"),Service->GetSelectedMatchmakingMode(),FName(TEXT("Arena.Mode.Team3v3")));
    Service->CompleteConfirmedTicketState(TEXT("unconfirmed-start"),EGamePlatformArenaClientFlowState::Idle);
    TestTrue(TEXT("确认旧报名已结束后允许选择1v1"),Service->SelectMatchmakingMenuMode(TEXT("Arena.Mode.Duel1v1")));
    return true;
}

/** 页面需求与业务快照分离：关闭页面不能取消票据，同状态更新不能弹回已关闭页面。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaMenuDemandRegressionTest,
    "DivineBeasts.Arena.UI.MatchmakingMenuDemand",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaMenuDemandRegressionTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UDivineBeastsArenaUIClientSubsystem> Service(NewObject<UDivineBeastsArenaUIClientSubsystem>(Player.Get()));
    Service->ArenaViewModel=NewObject<UGamePlatformArenaViewModel>(Service.Get());
    Service->bMatchmakingMenuRequested=true;
    Service->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Idle);
    TestEqual(TEXT("服务端Idle通知不关闭主动浏览"),Service->ResolvePrimaryArenaSurfaceId(),FName(TEXT("UI.Screen.Matchmaking")));
    Service->MatchmakingLogicalRequestId=TEXT("active-queue-request");
    const auto Adapter=MakeShared<FIncompleteMatchmakingTestAdapter>();Adapter->ObservedState=EGamePlatformArenaClientFlowState::Matchmaking;
    Service->RegisterMatchmakingAdapter(Adapter);
    Service->DismissMatchmakingMenu();
    Service->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Matchmaking);
    TestEqual(TEXT("关闭后队列通知保留非模态状态HUD"),Service->ResolvePrimaryArenaSurfaceId(),FName(TEXT("UI.HUD.ArenaQueueStatus")));
    TestEqual(TEXT("关闭不释放报名身份"),Service->MatchmakingLogicalRequestId,FString(TEXT("active-queue-request")));
    TestEqual(TEXT("关闭不调用取消副作用"),Adapter->Mutations,0);
    Service->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::MatchFound);
    TestEqual(TEXT("真实候选仍进入逐人确认页面"),Service->ResolvePrimaryArenaSurfaceId(),FName(TEXT("UI.Screen.MatchFoundReady")));
    Service->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Transferring);
    TestTrue(TEXT("迁移时队列HUD隐藏"),Service->ResolvePrimaryArenaSurfaceId().IsNone());
    Adapter->ObservedState=EGamePlatformArenaClientFlowState::Idle;
    Service->SetObservedArenaClientFlowState(EGamePlatformArenaClientFlowState::Idle);
    TestTrue(TEXT("确认Idle后队列HUD隐藏"),Service->ResolvePrimaryArenaSurfaceId().IsNone());
    return true;
}

/** 原生命名控件接线：页签切换可见性且失活解绑，未认证控件不得发送或回复邀请。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaChallengeWidgetRegressionTest,
    "DivineBeasts.Arena.UI.ChallengePanelLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaChallengeWidgetRegressionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UDivineBeastsMatchmakingScreen> Page(NewObject<UDivineBeastsMatchmakingScreen>());
    Page->WidgetTree=NewObject<UWidgetTree>(Page.Get());
    auto* Root=Page->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("Root"));Page->WidgetTree->RootWidget=Root;
    auto* Quick=Page->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("QuickMatchTabButton"));Root->AddChild(Quick);
    auto* Challenge=Page->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ChallengeTabButton"));Root->AddChild(Challenge);
    auto* Panel=Page->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("ChallengePanel"));Root->AddChild(Panel);
    auto* Input=Page->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("TargetPlayerIdInput"));Panel->AddChild(Input);
    auto* Send=Page->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("SendChallengeButton"));Panel->AddChild(Send);
    Page->BindUIEvents();Page->RefreshInitialState();
    TestEqual(TEXT("默认快速页不显示约战表单"),Panel->GetVisibility(),ESlateVisibility::Collapsed);
    Challenge->OnClicked.Broadcast();
    TestEqual(TEXT("约战按钮显示同页表单"),Panel->GetVisibility(),ESlateVisibility::Visible);
    Input->SetText(FText::FromString(TEXT("public-opponent")));
    TestFalse(TEXT("输入ID不能绕过认证资格"),Send->GetIsEnabled());
    Quick->OnClicked.Broadcast();
    TestEqual(TEXT("快速页按钮关闭约战表单"),Panel->GetVisibility(),ESlateVisibility::Collapsed);
    Page->UnbindUIEvents();
    TestFalse(TEXT("失活撤销页签点击"),Challenge->OnClicked.IsBound());
    TestFalse(TEXT("失活撤销输入变化观察"),Input->OnTextChanged.IsBound());
    return true;
}
#endif
