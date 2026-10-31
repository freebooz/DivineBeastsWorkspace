// 唯一匹配WBP的两种报名入口；Widget只投影快照并提交意图，不持有HTTP、邀请权威或旅行票据。
#include "UI/Screens/DivineBeastsMatchmakingScreen.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

void UDivineBeastsMatchmakingScreen::BindUIEvents()
{
    Super::BindUIEvents();
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("QuickMatchTabButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectQuickMatchTab);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("ChallengeTabButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectChallengeTab);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("SendChallengeButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::SendChallenge);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("AcceptChallengeButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::AcceptChallenge);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("DeclineChallengeButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::DeclineChallenge);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CancelChallengeButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::CancelChallenge);
    if(auto* Input=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TargetPlayerIdInput"))))Input->OnTextChanged.AddUniqueDynamic(this,&ThisClass::TargetPlayerIdChanged);
}
void UDivineBeastsMatchmakingScreen::UnbindUIEvents()
{
    for(const TCHAR* Name:{TEXT("QuickMatchTabButton"),TEXT("ChallengeTabButton"),TEXT("SendChallengeButton"),TEXT("AcceptChallengeButton"),TEXT("DeclineChallengeButton"),TEXT("CancelChallengeButton")})
        if(auto* Button=Cast<UButton>(GetWidgetFromName(Name)))Button->OnClicked.RemoveAll(this);
    if(auto* Input=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TargetPlayerIdInput"))))Input->OnTextChanged.RemoveAll(this);
    bChallengeTabSelected=false;LastDisplayedChallengeId.Empty();Super::UnbindUIEvents();
}
void UDivineBeastsMatchmakingScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    const auto* Client=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsArenaClientSubsystem>():nullptr;
    const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    const auto Challenge=Client?Client->GetObservedChallenge():FDivineBeastsArenaChallengeView{};
    if(Challenge.bPresent&&LastDisplayedChallengeId!=Challenge.ChallengeId)
    {LastDisplayedChallengeId=Challenge.ChallengeId;bChallengeTabSelected=true;}
    const auto Visible=bChallengeTabSelected?ESlateVisibility::Visible:ESlateVisibility::Collapsed;
    const auto QuickVisible=bChallengeTabSelected?ESlateVisibility::Collapsed:ESlateVisibility::Visible;
    if(auto* Panel=Cast<UPanelWidget>(GetWidgetFromName(TEXT("ChallengePanel"))))Panel->SetVisibility(Visible);
    if(auto* Panel=Cast<UPanelWidget>(GetWidgetFromName(TEXT("QuickMatchPanel"))))Panel->SetVisibility(QuickVisible);
    for(const TCHAR* Name:{TEXT("StartMatchmakingButton"),TEXT("CancelMatchmakingButton")})
        if(auto* Widget=GetWidgetFromName(Name))Widget->SetVisibility(QuickVisible);
    auto Text=[this](const TCHAR* Name,const FText& Value)
    {if(auto* Widget=Cast<UTextBlock>(GetWidgetFromName(Name))){Widget->SetText(Value);auto Font=Widget->GetFont();Font.Size=10;Widget->SetFont(Font);}};
    FString Reason;
    if(Challenge.bPresent)
    {
        Text(TEXT("ChallengeStatusText"),FText::FromString(FString::Printf(TEXT("约战邀请 · %s · 截止 %s UTC\n接受后全体玩家仍须分别确认；关闭页面不取消邀请"),*Challenge.ModeId,*Challenge.DeadlineUtc.ToString(TEXT("%H:%M:%S")))));
        Text(TEXT("ChallengeRosterText"),FText::FromString(FString::Printf(TEXT("甲队：%s\n乙队：%s"),*FString::Join(Challenge.TeamAPlayerIds,TEXT("、")),*FString::Join(Challenge.TeamBPlayerIds,TEXT("、")))));
    }
    else
    {
        const bool bModeReady=Client&&Client->CanChallengeMode(SelectedMode,Reason);
        Text(TEXT("ChallengeStatusText"),FText::FromString(bModeReady?TEXT("输入对方公开玩家ID；1v1可无队伍，多人约战须双方满编。邀请60秒，接受后全员确认。"):
            Reason.IsEmpty()?TEXT("正在同步约战状态"):Reason));
        Text(TEXT("ChallengeRosterText"),FText::FromString(TEXT("对手名单以服务器邀请快照为准；当前未接入好友列表。")));
    }
    FString Target;
    if(auto* Input=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TargetPlayerIdInput"))))
    {Target=Input->GetText().ToString();Input->SetIsEnabled(!Challenge.bPresent&&Service&&Service->CanRequestMatchmaking());auto Style=Input->GetWidgetStyle();Style.TextStyle.Font.Size=10;Input->SetWidgetStyle(Style);}
    bool bTargetValid=!Target.IsEmpty()&&Target.Len()<=128&&Target.TrimStartAndEnd()==Target;
    for(const TCHAR Character:Target)if(Character<32||Character==127)bTargetValid=false;
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("SendChallengeButton"))))Button->SetIsEnabled(Service&&bTargetValid&&Service->CanSendChallenge(SelectedMode));
    for(const TCHAR* Name:{TEXT("AcceptChallengeButton"),TEXT("DeclineChallengeButton")})
        if(auto* Button=Cast<UButton>(GetWidgetFromName(Name))){Button->SetIsEnabled(Service&&Service->CanReplyChallenge());Button->SetVisibility(Challenge.bPresent&&Challenge.bCanReply?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CancelChallengeButton"))))
    {Button->SetIsEnabled(Service&&Service->CanCancelChallengeInvitation());Button->SetVisibility(Challenge.bPresent&&Challenge.bCanCancel?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
}
void UDivineBeastsMatchmakingScreen::SelectQuickMatchTab(){bChallengeTabSelected=false;RefreshInitialState();}
void UDivineBeastsMatchmakingScreen::SelectChallengeTab(){bChallengeTabSelected=true;RefreshInitialState();}
void UDivineBeastsMatchmakingScreen::TargetPlayerIdChanged(const FText&){RefreshInitialState();}
void UDivineBeastsMatchmakingScreen::SendChallenge()
{
    auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    const auto* Input=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TargetPlayerIdInput")));
    if(Service&&Input)Service->SendChallenge(SelectedMode,Input->GetText().ToString());
    RefreshInitialState();
}
void UDivineBeastsMatchmakingScreen::AcceptChallenge()
{if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)Service->ReplyChallenge(true);RefreshInitialState();}
void UDivineBeastsMatchmakingScreen::DeclineChallenge()
{if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)Service->ReplyChallenge(false);RefreshInitialState();}
void UDivineBeastsMatchmakingScreen::CancelChallenge()
{if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)Service->CancelChallengeInvitation();RefreshInitialState();}
