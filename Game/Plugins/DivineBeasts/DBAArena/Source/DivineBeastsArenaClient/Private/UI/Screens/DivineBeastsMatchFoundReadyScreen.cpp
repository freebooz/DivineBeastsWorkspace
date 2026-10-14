// 候选确认页不调用场内SetArenaReady，不生成确认结果/票据，不以关闭窗口冒充拒绝。
#include "UI/Screens/DivineBeastsMatchFoundReadyScreen.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Client/DivineBeastsArenaUIClientSubsystem.h"
void UDivineBeastsMatchFoundReadyScreen::BindUIEvents()
{
    Super::BindUIEvents();
    if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())
        Service->OnMatchmakingCommandChanged.AddUObject(this,&UDivineBeastsMatchFoundReadyScreen::RefreshInitialState);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ReadyButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsMatchFoundReadyScreen::AcceptOffer);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("DeclineButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsMatchFoundReadyScreen::DeclineOffer);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton"))))B->OnClicked.AddUniqueDynamic(this,&UDivineBeastsMatchFoundReadyScreen::CloseOfferPage);
}
void UDivineBeastsMatchFoundReadyScreen::UnbindUIEvents()
{
    if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())Service->OnMatchmakingCommandChanged.RemoveAll(this);
    for(const TCHAR* Name:{TEXT("ReadyButton"),TEXT("DeclineButton"),TEXT("CloseButton")})if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->OnClicked.RemoveAll(this);
    Super::UnbindUIEvents();
}
void UDivineBeastsMatchFoundReadyScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    const auto* Service=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>():nullptr;
    const bool bCanRespond=Service&&Service->CanRespondToMatchOffer();
    for(const TCHAR* Name:{TEXT("ReadyButton"),TEXT("DeclineButton")})if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->SetIsEnabled(bCanRespond);
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("EntryStatusText"))))T->SetText(FText::FromString(bCanRespond?TEXT("请选择接受或拒绝；服务器确认后由既有应用流程处理准入"):TEXT("尚无可回复的服务器候选合同；无法接受或拒绝")));
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("CommandErrorText"))))T->SetText(Service?Service->GetMatchmakingCommandFeedback():FText::FromString(TEXT("本地玩家竞技命令服务不可用")));
}
void UDivineBeastsMatchFoundReadyScreen::AcceptOffer()
{if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())Service->RespondToMatchOffer(true);}
void UDivineBeastsMatchFoundReadyScreen::DeclineOffer()
{if(auto* Player=GetOwningLocalPlayer())if(auto* Service=Player->GetSubsystem<UDivineBeastsArenaUIClientSubsystem>())Service->RespondToMatchOffer(false);}
void UDivineBeastsMatchFoundReadyScreen::CloseOfferPage(){CloseScreen();}
