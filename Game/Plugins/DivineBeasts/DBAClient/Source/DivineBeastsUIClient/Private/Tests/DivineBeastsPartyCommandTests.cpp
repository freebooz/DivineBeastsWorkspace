// 公共UI命令授权回归：没有已认证平台来源时，页面/目标/本地选择都不能自行授予写权限。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ViewModels/Social/DivineBeastsPartyViewModel.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Social/DivineBeastsPartyScreenBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ListView.h"
#include "Widgets/Social/DivineBeastsPartyListEntry.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPartyCommandGateTest,
    "DivineBeasts.UI.Party.UnauthenticatedCommands",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPartyCommandGateTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsPartyViewModel> Model(NewObject<UDivineBeastsPartyViewModel>());
    Model->BeginPage();Model->SetTargetContext(TEXT("target"),TEXT("party"),7);
    for(const FString Action:{TEXT("invite"),TEXT("ready"),TEXT("acceptRequest"),TEXT("requestJoin"),TEXT("disband")})
    {
        TestFalse(TEXT("目标上下文不能授予未认证写权限"),Model->CanSubmitPartyAction(Action));
        TestFalse(TEXT("未认证命令不返回受理成功"),Model->SubmitPartyAction(Action,TEXT("target"),TEXT("request")));
    }
    TestFalse(TEXT("没有未知命令不能产生新重试"),Model->RetryUnknownCommand());
    Model->EndPage();TestFalse(TEXT("页面失活不能写入"),Model->CanSubmitPartyAction(TEXT("ready")));
    Model->DisconnectPlayer();TestTrue(TEXT("解绑清空本人身份"),Model->CurrentPlayerId.IsEmpty());
    return true;
}

/** 仅内存Widget夹具，不生成资产：覆盖真实选择委托、暂时覆盖与同Widget换模型重用。
 * 该用例不联网、不赋予认证，不替代真实Root/CommonUI栈联调。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPartyBindingRegressionTest,
    "DivineBeasts.UI.Party.SelectionAndRecycledBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPartyBindingRegressionTest::RunTest(const FString&)
{
    FScopedAllowAbstractClassAllocation AllowAbstract;
    const TStrongObjectPtr<UGamePlatformUIScreen> Page(NewObject<UGamePlatformUIScreen>());
    const TStrongObjectPtr<UDivineBeastsPartyViewModel> First(NewObject<UDivineBeastsPartyViewModel>());
    const TStrongObjectPtr<UDivineBeastsPartyViewModel> Second(NewObject<UDivineBeastsPartyViewModel>());
    const TStrongObjectPtr<UDivineBeastsPartyScreenBinding> Binding(NewObject<UDivineBeastsPartyScreenBinding>());
    Page->WidgetTree=NewObject<UWidgetTree>(Page.Get());
    auto* List=Page->WidgetTree->ConstructWidget<UListView>(UListView::StaticClass(),TEXT("PartyMemberList"));
    Page->WidgetTree->RootWidget=List;
    FGamePlatformPartyMember MemberA;MemberA.PlayerId=TEXT("A");
    FGamePlatformPartyMember MemberB;MemberB.PlayerId=TEXT("B");
    First->Party.Members={MemberA,MemberB};
    Page->InitializeScreen(TEXT("UI.Party.Panel"),First.Get(),NAME_None,EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    Binding->Bind(Page.Get(),First.Get(),nullptr);
    auto* A=NewObject<UDivineBeastsPartyListItem>(Page.Get());A->Key=TEXT("member:A");
    auto* B=NewObject<UDivineBeastsPartyListItem>(Page.Get());B->Key=TEXT("member:B");
    // 键盘/手柄走SelectionChanged而非ItemClicked；广播真实原生事件以覆盖接线合同。
    List->OnItemSelectionChanged().Broadcast(A);
    TestEqual(TEXT("首次选择同步A"),Binding->SelectedMember,FString(TEXT("A")));
    List->OnItemSelectionChanged().Broadcast(B);
    TestEqual(TEXT("键盘移动后命令缓存必须同步B"),Binding->SelectedMember,FString(TEXT("B")));
    List->OnItemSelectionChanged().Broadcast(nullptr);
    TestTrue(TEXT("取消选择撤销命令目标"),Binding->SelectedMember.IsEmpty());
    Binding->Deactivated();
    TestTrue(TEXT("只是暂时覆盖不能退休同模型绑定"),Binding->IsBound());
    Page->InitializeScreen(TEXT("UI.Party.Panel"),Second.Get(),NAME_None,EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    Binding->Activated();
    TestFalse(TEXT("池化同Widget换模型必须退休旧绑定"),Binding->IsBound());
    TestFalse(TEXT("退休解除列表选择委托"),List->OnItemSelectionChanged().IsBoundToObject(Binding.Get()));
    Binding->Unbind();
    return true;
}
#endif
