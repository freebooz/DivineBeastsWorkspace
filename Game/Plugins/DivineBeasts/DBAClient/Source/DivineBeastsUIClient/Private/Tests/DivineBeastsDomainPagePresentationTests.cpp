// SSLM社交/公告真实页面事件回归；仅Editor显式夹具提供脱敏快照，不接生产服务或伪造人工签收。
// 验证正常投影、来源撤销、活动换VM；把事件接线改为Tick或漏掉旧模型清理将导致可见文本断言失败。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Screens/Social/DivineBeastsSocialScreenBase.h"
#include "Screens/LiveOps/DivineBeastsLiveOpsScreenBase.h"
#include "ViewModels/Social/DivineBeastsSocialViewModel.h"
#include "ViewModels/LiveOps/DivineBeastsLiveOpsViewModel.h"
#include "Screens/World/DivineBeastsWorldJournalScreen.h"
#include "ViewModels/World/DivineBeastsWorldJournalViewModel.h"
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDomainPagePresentationTest,
    "DivineBeasts.UI.RemainingSurfaces.DomainEvents",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDomainPagePresentationTest::RunTest(const FString&)
{
    UClass* SocialClass=LoadClass<UDivineBeastsSocialScreenBase>(nullptr,TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Social.WBP_DBA_UI_Social_C"));
    UClass* NoticesClass=LoadClass<UDivineBeastsLiveOpsScreenBase>(nullptr,TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_LiveOps.WBP_DBA_UI_LiveOps_C"));
    if(!TestNotNull(TEXT("真实社交资产"),SocialClass)||!TestNotNull(TEXT("真实公告资产"),NoticesClass))return false;
    TStrongObjectPtr<UDivineBeastsSocialScreenBase> Social(NewObject<UDivineBeastsSocialScreenBase>(GetTransientPackage(),SocialClass));
    TStrongObjectPtr<UDivineBeastsSocialViewModel> SocialModel(NewObject<UDivineBeastsSocialViewModel>());
    const FGuid Scope=FGuid::NewGuid();SocialModel->BeginSourceScope(Scope);
    Social->Initialize();Social->InitializeScreen(TEXT("UI.Screen.Social"),SocialModel.Get(),TEXT("FriendsButton"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);Social->ActivateWidget();
    FDivineBeastsSocialUIProjection Projection;Projection.SourceScopeId=Scope;Projection.Revision=1;Projection.bServiceAvailable=true;
    FDivineBeastsSocialMemberView Friend;Friend.PublicPlayerId=TEXT("test-public-player");Friend.DisplayName=FText::FromString(TEXT("审核夹具好友"));Friend.bOnline=true;Projection.Friends.Add(Friend);
    TestTrue(TEXT("授权夹具快照被接纳"),SocialModel->ApplySocialSnapshot(Projection));
    auto* SocialText=Cast<UTextBlock>(Social->GetWidgetFromName(TEXT("SocialDetailText")));
    if(TestNotNull(TEXT("社交详情文本"),SocialText))
    {
        TestTrue(TEXT("不需Tick即可显示真实输入事件"),SocialText->GetText().ToString().Contains(TEXT("审核夹具好友")));
        SocialModel->ClearSourceScope();
        TestFalse(TEXT("来源撤销立即清除旧昵称"),SocialText->GetText().ToString().Contains(TEXT("审核夹具好友")));
    }
    Social->DeactivateWidget();
    TStrongObjectPtr<UDivineBeastsLiveOpsScreenBase> Notices(NewObject<UDivineBeastsLiveOpsScreenBase>(GetTransientPackage(),NoticesClass));
    TStrongObjectPtr<UDivineBeastsLiveOpsViewModel> NoticesModel(NewObject<UDivineBeastsLiveOpsViewModel>());
    NoticesModel->BeginSourceScope(Scope);Notices->Initialize();Notices->InitializeScreen(TEXT("UI.Screen.LiveOps"),NoticesModel.Get(),TEXT("NoticesButton"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);Notices->ActivateWidget();
    FDivineBeastsLiveOpsUIProjection Ops;Ops.SourceScopeId=Scope;Ops.Revision=1;Ops.bServiceAvailable=true;Ops.UnreadCount=1;
    FDivineBeastsLiveOpsNoticeView Notice;Notice.NoticeId=TEXT("test-notice");Notice.Title=FText::FromString(TEXT("审核夹具公告"));Notice.Summary=FText::FromString(TEXT("正文来自事件快照"));Ops.Notices.Add(Notice);
    TestTrue(TEXT("公告夹具通过来源门禁"),NoticesModel->ApplyLiveOpsSnapshot(Ops));
    auto* Body=Cast<UTextBlock>(Notices->GetWidgetFromName(TEXT("NoticeBodyText")));
    if(TestNotNull(TEXT("公告正文"),Body))TestTrue(TEXT("新快照立即更新正文"),Body->GetText().ToString().Contains(TEXT("正文来自事件快照")));
    TStrongObjectPtr<UDivineBeastsLiveOpsViewModel> Replacement(NewObject<UDivineBeastsLiveOpsViewModel>());
    Notices->InitializeScreen(TEXT("UI.Screen.LiveOps"),Replacement.Get(),TEXT("NoticesButton"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    if(Body)TestFalse(TEXT("活动换模型不遗留上一账号公告"),Body->GetText().ToString().Contains(TEXT("正文来自事件快照")));
    Notices->DeactivateWidget();return true;
}

// 活动手册通过平台公开入口清空VM后，旧列表/文本/追踪命令和地图快照必须一并撤销。
// 夹具不伪造本地玩家或世界准入；地图用已公开的只读投影输入验证来源清理确实抵达组件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsJournalClearedModelTest,
    "DivineBeasts.UI.RemainingSurfaces.JournalClearedModel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsJournalClearedModelTest::RunTest(const FString&)
{
    UClass* Class=LoadClass<UDivineBeastsWorldJournalScreen>(nullptr,TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_WorldJournal.WBP_DBA_UI_WorldJournal_C"));
    if(!TestNotNull(TEXT("真实世界手册资产"),Class))return false;
    TStrongObjectPtr<UDivineBeastsWorldJournalScreen> Page(NewObject<UDivineBeastsWorldJournalScreen>(GetTransientPackage(),Class));
    TStrongObjectPtr<UDivineBeastsWorldJournalViewModel> Model(NewObject<UDivineBeastsWorldJournalViewModel>());
    Page->Initialize();Page->InitializeScreen(TEXT("UI.Screen.WorldJournal"),Model.Get(),TEXT("CharacterTab"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);Page->ActivateWidget();
    auto* Entries=Cast<UComboBoxString>(Page->GetWidgetFromName(TEXT("EntryList")));
    auto* Summary=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("SummaryText")));
    auto* Details=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("DetailText")));
    auto* Track=Cast<UButton>(Page->GetWidgetFromName(TEXT("TrackButton")));
    auto* Untrack=Cast<UButton>(Page->GetWidgetFromName(TEXT("UntrackButton")));
    auto* Map=Cast<UDivineBeastsVillageMinimapWidget>(Page->GetWidgetFromName(TEXT("WorldMap")));
    if(!TestNotNull(TEXT("条目列表"),Entries)||!TestNotNull(TEXT("概览"),Summary)||!TestNotNull(TEXT("详情"),Details)||!TestNotNull(TEXT("追踪"),Track)||!TestNotNull(TEXT("取消追踪"),Untrack)||!TestNotNull(TEXT("地图组件"),Map))return false;
    Entries->AddOption(TEXT("旧条目夹具"));Summary->SetText(FText::FromString(TEXT("旧概要夹具")));Details->SetText(FText::FromString(TEXT("旧详情夹具")));
    Track->SetIsEnabled(true);Untrack->SetIsEnabled(true);
    FGamePlatformUIMinimapState State;State.MapId=TEXT("Fixture.OldWorld");State.Revision=1;State.Markers.AddDefaulted_GetRef().MarkerId=TEXT("Fixture.Marker");
    TestTrue(TEXT("可见地图夹具已安装"),Map->ApplyMinimapState(State));Map->SetVisibility(ESlateVisibility::Visible);
    Page->InitializeScreen(TEXT("UI.Screen.WorldJournal"),nullptr,TEXT("CharacterTab"),EGamePlatformUIInputMode::GameAndUI,EGamePlatformUIPausePolicy::Never);
    TestEqual(TEXT("清空模型撤下列表"),Entries->GetOptionCount(),0);
    TestFalse(TEXT("清空模型撤下旧概要"),Summary->GetText().ToString().Contains(TEXT("旧概要")));
    TestTrue(TEXT("清空模型撤下旧详情"),Details->GetText().IsEmpty());
    TestFalse(TEXT("失效追踪不能操作"),Track->GetIsEnabled());TestFalse(TEXT("失效取消追踪不能操作"),Untrack->GetIsEnabled());
    TestTrue(TEXT("清空模型隐藏地图"),Map->GetVisibility()==ESlateVisibility::Collapsed);
    TestTrue(TEXT("地图来源撤销清除旧标记"),Map->GetMinimapStateView().Markers.IsEmpty());
    const FString EmptySummary=Summary->GetText().ToString();
    Model->OnViewStateChanged.Broadcast(Model->GetRevision(),Model->GetPageGeneration());
    TestEqual(TEXT("旧模型广播不得恢复手册"),Summary->GetText().ToString(),EmptySummary);
    Page->DeactivateWidget();return true;
}
#endif
