// 项目竞技页面原生命令接线；下层MOBA持有复制投影，平台持有页面生命周期，后端缺失能力失败关闭。
#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "ViewModels/GamePlatformArenaViewModel.h"
#include "Widgets/Social/DivineBeastsPartyListEntry.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"

void UDivineBeastsArenaScreenBase::BindUIEvents()
{
    Super::BindUIEvents();
    if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())
        Service->OnMatchmakingCommandChanged.AddUObject(this,&UDivineBeastsArenaScreenBase::RefreshInitialState);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("StartMatchmakingButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsArenaScreenBase::StartMatchmaking);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsArenaScreenBase::CancelMatchmaking);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ReturnWorldButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsArenaScreenBase::ReturnWorld);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("RetryReturnButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsArenaScreenBase::ReturnWorld);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsArenaScreenBase::CloseArenaPage);
    if(auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ModeList"))))List->OnItemSelectionChanged().AddUObject(this,&UDivineBeastsArenaScreenBase::ModeSelected);
}
void UDivineBeastsArenaScreenBase::UnbindUIEvents()
{
    if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())Service->OnMatchmakingCommandChanged.RemoveAll(this);
    for(const TCHAR* Name:{TEXT("StartMatchmakingButton"),TEXT("CancelMatchmakingButton"),TEXT("ReturnWorldButton"),TEXT("RetryReturnButton"),TEXT("CloseButton")})
        if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->OnClicked.RemoveAll(this);
    if(auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ModeList"))))List->OnItemSelectionChanged().RemoveAll(this);
    SelectedMode=NAME_None;CommandFeedback=FText();Super::UnbindUIEvents();
}
void UDivineBeastsArenaScreenBase::ModeSelected(UObject* Object)
{
    if(bRebuildingModes)return;
    const auto* Item=Cast<UDivineBeastsPartyListItem>(Object);
    const FName Requested=Item&&FDivineBeastsArenaModeCatalog::Find(FName(*Item->Key))?FName(*Item->Key):NAME_None;
    auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    if(ExpectedSurfaceId==FName(TEXT("UI.Screen.Matchmaking"))&&Service)
    {Service->SelectMatchmakingMenuMode(Requested);SelectedMode=Service->GetSelectedMatchmakingMode();}
    else SelectedMode=Requested;
    RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::RefreshInitialState()
{
    Super::RefreshInitialState();
    if(ExpectedSurfaceId==FName(TEXT("UI.Screen.Matchmaking")))
        if(const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)
            SelectedMode=Service->GetSelectedMatchmakingMode();
    auto Text=[this](const TCHAR* Name,const FText& Value){if(auto* T=Cast<UTextBlock>(GetWidgetFromName(Name)))T->SetText(Value);};
    auto* VM=GetArenaViewModel();
    if(auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ModeList"))))
    {
        TGuardValue<bool> Rebuilding(bRebuildingModes,true);
        List->ClearListItems();
        for(const auto& Mode:FDivineBeastsArenaModeCatalog::GetAll())
        {
            auto* Item=NewObject<UDivineBeastsPartyListItem>(this);Item->Key=Mode.ArenaModeId.ToString();
            Item->Title=FText::FromString(FString::Printf(TEXT("%d 对 %d"),Mode.TeamSize,Mode.TeamSize));
            FString Reason;const bool bReady=Mode.ValidateRuntime(FDivineBeastsArenaModeCatalog::IsDevelopmentEnabledForCurrentProcess(),Reason);
            Item->Detail=FText::FromString(bReady?TEXT("单局淘汰 · 小队保持完整"):TEXT("当前客户端未启用此模式"));
            List->AddItem(Item);if(SelectedMode==Mode.ArenaModeId)List->SetSelectedItem(Item);
        }
    }
    Text(TEXT("SelectedModeText"),FText::FromString(SelectedMode.IsNone()?TEXT("请选择竞技模式"):FString::Printf(TEXT("%d 对 %d"),FDivineBeastsArenaModeCatalog::Find(SelectedMode)?FDivineBeastsArenaModeCatalog::Find(SelectedMode)->TeamSize:0,FDivineBeastsArenaModeCatalog::Find(SelectedMode)?FDivineBeastsArenaModeCatalog::Find(SelectedMode)->TeamSize:0)));
    const auto* Client=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsArenaClientSubsystem>():nullptr;
    Text(TEXT("SearchStatusText"),Client?Client->GetObservedMatchmakingStatus():FText::FromString(TEXT("正在连接匹配服务")));
    Text(TEXT("ElapsedTimeText"),FText::GetEmpty());
    const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    Text(TEXT("CommandErrorText"),Service&&!Service->GetMatchmakingCommandFeedback().IsEmpty()?Service->GetMatchmakingCommandFeedback():CommandFeedback);
    // 没有取消/观察能力时不能先产生不可见、不可撤销的队列预留；不发送一个注定无法闭环的报名。
    const auto* Selected=FDivineBeastsArenaModeCatalog::Find(SelectedMode);FString ConfigurationError;
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("StartMatchmakingButton"))))B->SetIsEnabled((Client&&Client->CanRetryObservedMigration())||(Service&&Service->CanRequestMatchmaking()&&Selected&&Client&&Client->CanQueueMode(SelectedMode,ConfigurationError)));
    if(!ConfigurationError.IsEmpty())Text(TEXT("CommandErrorText"),FText::FromString(ConfigurationError));
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton"))))B->SetIsEnabled(Service&&Service->CanCancelMatchmaking());
    Text(TEXT("StartMatchmakingButton_Label"),FText::FromString(Client&&Client->CanRetryObservedMigration()?TEXT("重试连接"):TEXT("开始匹配")));
    const bool bCanReturn=Client&&Client->CanReturnObservedMatch();
    for(const TCHAR* Name:{TEXT("ReturnWorldButton"),TEXT("RetryReturnButton")})if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->SetIsEnabled(bCanReturn);
    Text(TEXT("ResultTitle"),FText::FromString(!VM||VM->ResultSummary.EndRevision<=0?TEXT("尚无已确认比赛结果"):VM->ResultSummary.WinningTeamId.IsNone()?TEXT("比赛结束 · 平局"):TEXT("比赛结束")));
    Text(TEXT("ReturnStatusText"),CommandFeedback);
    // 英雄选择与记分板共享服务器公开名单；本轮沿用后端锁定英雄，不需要二次选人。
    Text(TEXT("ArenaStateText"),FText::FromString(VM&&!VM->MatchId.IsEmpty()
        ? FString::Printf(TEXT("比赛 %s · 模式 %s"),*VM->MatchId,*VM->ArenaModeId.ToString())
        : TEXT("尚未收到服务器确认的竞技状态")));
    FString Roster;
    if(VM)for(const auto& Row:VM->Scoreboard)
        Roster+=FString::Printf(TEXT("%s · 队伍 %s · 英雄 %s · %s\n"),*Row.PlayerId,*Row.TeamId.ToString(),
            *Row.HeroDefinitionId,Row.bReady?TEXT("已准备"):TEXT("未准备"));
    Text(TEXT("RosterText"),FText::FromString(Roster.IsEmpty()?TEXT("尚无已复制的公开阵容"):Roster));
    Text(TEXT("HeroSelectionStatusText"),FText::FromString(TEXT("本轮使用匹配确认时已锁定的英雄；全员进入后倒计时开始，无需再次选人。")));
    // 在真实选人适配器注册之前失败关闭；不能把点击或本地选中当作服务端确认。
    for(const TCHAR* Name:{TEXT("ConfirmHeroButton"),TEXT("ReadyButton")})
        if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->SetIsEnabled(false);
    if(auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ResultsList"))))
    {
        List->ClearListItems();
        if(VM)for(const auto& Row:VM->Scoreboard)
        {
            auto* Item=NewObject<UDivineBeastsPartyListItem>(this);Item->Key=Row.PlayerId;Item->Title=FText::FromString(Row.PlayerId);
            Item->Detail=FText::FromString(FString::Printf(TEXT("队伍 %s · 击杀 %d · 死亡 %d · 助攻 %d · 得分 %d"),*Row.TeamId.ToString(),Row.Kills,Row.Deaths,Row.Assists,Row.Score));List->AddItem(Item);
        }
    }
}
void UDivineBeastsArenaScreenBase::OnViewModelStateChanged(int32 Revision,int32 Generation)
{Super::OnViewModelStateChanged(Revision,Generation);RefreshInitialState();}
void UDivineBeastsArenaScreenBase::StartMatchmaking()
{
    // 真实后端状态决定同一按钮的明确意图；失败恢复不重复报名，也不让隐藏控件承担无法触达的重试。
    if(auto* Client=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsArenaClientSubsystem>():nullptr)
        if(Client->CanRetryObservedMigration()){Client->RetryObservedMigration();RefreshInitialState();return;}
    const auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ModeList")));
    const auto* Item=List?Cast<UDivineBeastsPartyListItem>(List->GetSelectedItem()):nullptr;
    if(!Item||FName(*Item->Key)!=SelectedMode){CommandFeedback=FText::FromString(TEXT("请选择当前列表中的竞技模式"));RefreshInitialState();return;}
    auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    if(Service)Service->RequestMatchmaking(SelectedMode);
    else CommandFeedback=FText::FromString(TEXT("本地玩家竞技命令服务不可用"));
    RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::CancelMatchmaking()
{
    if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())Service->CancelMatchmaking();
    RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::ReturnWorld()
{
    const auto* VM=GetArenaViewModel();auto* Client=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsArenaClientSubsystem>():nullptr;
    const bool bAccepted=Client&&Client->RequestPostMatchReturnToWorld();
    CommandFeedback=FText::FromString(bAccepted?TEXT("返回请求已交给既有应用流程，等待后端新准入"):TEXT("当前结果或流程不允许返回；未使用旧地址或旧票据"));RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::CloseArenaPage()
{
    if(ExpectedSurfaceId==FName(TEXT("UI.Screen.Matchmaking")))
        if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)
        {Service->DismissMatchmakingMenu();return;}
    CloseScreen();
}
