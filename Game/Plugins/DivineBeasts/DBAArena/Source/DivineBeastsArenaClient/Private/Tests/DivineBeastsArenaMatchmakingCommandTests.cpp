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
#include "Widgets/Social/DivineBeastsPartyListEntry.h"

namespace
{
    class FIncompleteMatchmakingTestAdapter final : public IGamePlatformArenaMatchmakingAdapter
    {
    public:
        int32 Mutations = 0;
        virtual void RequestMatchmaking(const FGamePlatformArenaMatchmakingRequest&,TFunction<void(bool,const FString&)> Done) override
        {++Mutations;Done(false,TEXT("Test.UnexpectedMutation"));}
        virtual void CancelMatchmaking(const FString&,TFunction<void(bool,const FString&)> Done) override
        {++Mutations;Done(false,TEXT("Test.UnexpectedMutation"));}
        virtual EGamePlatformArenaClientFlowState GetObservedState() const override {return EGamePlatformArenaClientFlowState::Idle;}
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
#endif
