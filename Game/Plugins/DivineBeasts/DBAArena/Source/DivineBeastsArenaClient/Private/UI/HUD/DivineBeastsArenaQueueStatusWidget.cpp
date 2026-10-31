// 可选竞技HUD只复用本人观察与已有取消入口；平台普通HUD层拥有显示，不阻挡村庄行走。
#include "UI/HUD/DivineBeastsArenaQueueStatusWidget.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

void UDivineBeastsArenaQueueStatusWidget::BindUIEvents()
{
    Super::BindUIEvents();
    if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)
        Service->OnMatchmakingCommandChanged.AddUObject(this,&ThisClass::RefreshInitialState);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("OpenMatchmakingButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenMatchmaking);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton"))))Button->OnClicked.AddUniqueDynamic(this,&ThisClass::CancelMatchmaking);
}
void UDivineBeastsArenaQueueStatusWidget::UnbindUIEvents()
{
    if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)Service->OnMatchmakingCommandChanged.RemoveAll(this);
    for(const TCHAR* Name:{TEXT("OpenMatchmakingButton"),TEXT("CancelMatchmakingButton")})
        if(auto* Button=Cast<UButton>(GetWidgetFromName(Name)))Button->OnClicked.RemoveAll(this);
    Super::UnbindUIEvents();
}
void UDivineBeastsArenaQueueStatusWidget::RefreshInitialState()
{
    Super::RefreshInitialState();
    const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    const auto* Client=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsArenaClientSubsystem>():nullptr;
    const auto* Mode=Service?FDivineBeastsArenaModeCatalog::Find(Service->GetSelectedMatchmakingMode()):nullptr;
    auto Text=[this](const TCHAR* Name,const FText& Value)
    {if(auto* Widget=Cast<UTextBlock>(GetWidgetFromName(Name))){Widget->SetText(Value);auto Font=Widget->GetFont();Font.Size=10;Widget->SetFont(Font);}};
    Text(TEXT("QueueModeText"),FText::FromString(Mode?FString::Printf(TEXT("竞技场 · %d 对 %d"),Mode->TeamSize,Mode->TeamSize):TEXT("竞技状态")));
    Text(TEXT("QueueStatusText"),Client?Client->GetObservedMatchmakingStatus():FText::FromString(TEXT("正在同步竞技状态")));
    Text(TEXT("CommandErrorText"),Service?Service->GetMatchmakingCommandFeedback():FText::GetEmpty());
    const bool bCanCancelInvitation=Service&&Service->CanCancelChallengeInvitation();
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton"))))Button->SetIsEnabled(Service&&(bCanCancelInvitation||Service->CanCancelMatchmaking()));
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("OpenMatchmakingButton"))))Button->SetIsEnabled(Client&&Client->IsApplicationFlowInWorld());
    Text(TEXT("CancelMatchmakingButton_Label"),FText::FromString(bCanCancelInvitation?TEXT("取消约战"):TEXT("退出匹配")));
}
void UDivineBeastsArenaQueueStatusWidget::OnViewModelStateChanged(int32 Revision,int32 PageGeneration)
{Super::OnViewModelStateChanged(Revision,PageGeneration);RefreshInitialState();}
void UDivineBeastsArenaQueueStatusWidget::OpenMatchmaking()
{if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)Service->OpenMatchmakingMenu();}
void UDivineBeastsArenaQueueStatusWidget::CancelMatchmaking()
{
    if(auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr)
    {if(Service->CanCancelChallengeInvitation())Service->CancelChallengeInvitation();else Service->CancelMatchmaking();}
}
