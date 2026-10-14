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
    SelectedMode=Item&&FDivineBeastsArenaModeCatalog::Find(FName(*Item->Key))?FName(*Item->Key):NAME_None;
    RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::RefreshInitialState()
{
    Super::RefreshInitialState();
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
            FString Reason;const bool bReady=Mode.ValidateProduction(Reason);
            Item->Detail=FText::FromString(bReady?TEXT("规则已配置；仍需服务端能力确认"):TEXT("尚未配置正式地图与规则，不可报名"));
            List->AddItem(Item);if(SelectedMode==Mode.ArenaModeId)List->SetSelectedItem(Item);
        }
    }
    Text(TEXT("SelectedModeText"),FText::FromString(SelectedMode.IsNone()?TEXT("请选择竞技模式"):SelectedMode.ToString()));
    Text(TEXT("SearchStatusText"),FText::FromString(TEXT("尚未报名；当前后端未提供完整查询、取消与逐人确认合同")));
    Text(TEXT("ElapsedTimeText"),FText::GetEmpty());
    const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    Text(TEXT("CommandErrorText"),Service&&!Service->GetMatchmakingCommandFeedback().IsEmpty()?Service->GetMatchmakingCommandFeedback():CommandFeedback);
    // 没有取消/观察能力时不能先产生不可见、不可撤销的队列预留；不发送一个注定无法闭环的报名。
    const auto* Selected=FDivineBeastsArenaModeCatalog::Find(SelectedMode);FString ConfigurationError;
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("StartMatchmakingButton"))))B->SetIsEnabled(Service&&Service->CanRequestMatchmaking()&&Selected&&Selected->ValidateProduction(ConfigurationError));
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton"))))B->SetIsEnabled(Service&&Service->CanCancelMatchmaking());
    const bool bCanReturn=VM&&VM->MatchPhase==EGamePlatformArenaMatchPhase::Completed&&VM->ResultSummary.EndRevision>0;
    for(const TCHAR* Name:{TEXT("ReturnWorldButton"),TEXT("RetryReturnButton")})if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->SetIsEnabled(bCanReturn);
    Text(TEXT("ResultTitle"),FText::FromString(!VM||VM->ResultSummary.EndRevision<=0?TEXT("尚无已确认比赛结果"):VM->ResultSummary.WinningTeamId.IsNone()?TEXT("比赛结束 · 平局"):TEXT("比赛结束")));
    Text(TEXT("ReturnStatusText"),CommandFeedback);
    if(auto* List=Cast<UListView>(GetWidgetFromName(TEXT("ResultsList"))))
    {
        List->ClearListItems();
        if(VM)for(const auto& Row:VM->Scoreboard)
        {
            auto* Item=NewObject<UDivineBeastsPartyListItem>(this);Item->Key=Row.PlayerId;Item->Title=FText::FromString(Row.PlayerId);
            Item->Detail=FText::FromString(FString::Printf(TEXT("击杀 %d · 死亡 %d · 助攻 %d"),Row.Kills,Row.Deaths,Row.Assists));List->AddItem(Item);
        }
    }
}
void UDivineBeastsArenaScreenBase::OnViewModelStateChanged(int32 Revision,int32 Generation)
{Super::OnViewModelStateChanged(Revision,Generation);RefreshInitialState();}
void UDivineBeastsArenaScreenBase::StartMatchmaking()
{
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
    const bool bAccepted=VM&&VM->MatchPhase==EGamePlatformArenaMatchPhase::Completed&&VM->ResultSummary.EndRevision>0&&Client&&Client->RequestPostMatchReturnToWorld();
    CommandFeedback=FText::FromString(bAccepted?TEXT("返回请求已交给既有应用流程，等待后端新准入"):TEXT("当前结果或流程不允许返回；未使用旧地址或旧票据"));RefreshInitialState();
}
void UDivineBeastsArenaScreenBase::CloseArenaPage(){CloseScreen();}
