// 公共五页组合只注册平台软定义；可选竞技通过弱提供者接入，不加载MOBA模块或竞技资源。
#include "Social/DivineBeastsPartyUIClientSubsystem.h"
#include "Social/DivineBeastsPartyScreenBinding.h"
#include "ViewModels/Social/DivineBeastsPartyViewModel.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Party/GamePlatformPartyClientSubsystem.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FPartyPage {const TCHAR* Id;const TCHAR* Asset;const TCHAR* Focus;EGamePlatformUILayer Layer;};
    const FPartyPage Pages[]={
        {TEXT("UI.Party.Panel"),TEXT("WBP_DBA_PartyPanel"),TEXT("TargetPlayerInput"),EGamePlatformUILayer::Screen},
        {TEXT("UI.Party.Invitation"),TEXT("WBP_DBA_PartyInviteModal"),TEXT("InvitationsList"),EGamePlatformUILayer::Modal},
        {TEXT("UI.Party.Applications"),TEXT("WBP_DBA_PartyApplications"),TEXT("ApplicationsList"),EGamePlatformUILayer::Screen},
        {TEXT("UI.Party.Context"),TEXT("WBP_DBA_PlayerContextMenu"),TEXT("CloseButton"),EGamePlatformUILayer::System},
        {TEXT("UI.Party.Ready"),TEXT("WBP_DBA_PartyReadyCheckModal"),TEXT("CloseButton"),EGamePlatformUILayer::Modal}};
}
void UDivineBeastsPartyUIClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();
    UI=GetLocalPlayer()->GetSubsystem<UGamePlatformUIManagerSubsystem>(); if(!UI)return;
    UI->OnScreenOpened.AddDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenOpened);
    UI->OnScreenOpenFailed.AddDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenFailed);
    UI->OnScreenClosed.AddDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenClosed);
    for(const auto& Page:Pages)
    {
        auto* Definition=NewObject<UGamePlatformUIScreenDefinition>(this); Definition->ScreenId=Page.Id;
        Definition->WidgetClass=TSoftClassPtr<UGamePlatformUIScreen>(FSoftObjectPath(FString::Printf(TEXT("/DBAUIPack_Core/UI/Social/%s.%s_C"),Page.Asset,Page.Asset)));
        Definition->DefaultFocusWidgetName=Page.Focus; Definition->Layer=Page.Layer;
        Definition->InputMode=Page.Layer==EGamePlatformUILayer::Modal?EGamePlatformUIInputMode::UIOnly:EGamePlatformUIInputMode::GameAndUI;
        Definition->PausePolicy=EGamePlatformUIPausePolicy::Never;
        if(UI->RegisterScreenDefinition(Definition))Definitions.Add(Definition);
    }
    if(auto* Instance=GetLocalPlayer()->GetGameInstance())
    {
        PartySource=Instance->GetSubsystem<UGamePlatformPartyClientSubsystem>();
        ApplicationFlow=Instance->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>();
        if(auto* Source=PartySource.Get())PartyObservationHandle=Source->OnChanged().AddUObject(this,&UDivineBeastsPartyUIClientSubsystem::PartyObservationChanged);
        if(auto* Flow=ApplicationFlow.Get())FlowStateHandle=Flow->OnViewStateChanged().AddUObject(this,&UDivineBeastsPartyUIClientSubsystem::FlowStateChanged);
    }
    ProjectPartyNotifications();

}
void UDivineBeastsPartyUIClientSubsystem::Deinitialize()
{
    bClosing=true;
    if(auto* Source=PartySource.Get())Source->OnChanged().Remove(PartyObservationHandle);
    if(auto* Flow=ApplicationFlow.Get())Flow->OnViewStateChanged().Remove(FlowStateHandle);
    PartyObservationHandle.Reset();FlowStateHandle.Reset();PartySource.Reset();ApplicationFlow.Reset();SeenInvitationIds.Reset();SeenReadyCheckId.Empty();NotificationAuthGeneration.Invalidate();
    ActivityOwner.Reset(); ActivityAction={}; ActivityHandle.Invalidate();
    CloseOwnedPages();
    auto Pending=MoveTemp(PendingModels); PendingModels.Reset();
    if(UI)
    {
        UI->OnScreenOpened.RemoveDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenOpened);
        UI->OnScreenOpenFailed.RemoveDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenFailed);
        UI->OnScreenClosed.RemoveDynamic(this,&UDivineBeastsPartyUIClientSubsystem::ScreenClosed);
        for(const auto& Pair:Pending){UI->CancelOpen(Pair.Key);if(Pair.Value)Pair.Value->DisconnectPlayer();}
        for(const auto& Definition:Definitions)if(Definition)UI->UnregisterScreenDefinition(Definition->ScreenId);
    }
    for(const auto& Binding:Bindings)if(Binding)Binding->Unbind();
    Bindings.Reset(); Definitions.Reset(); UI=nullptr; Super::Deinitialize();
}
void UDivineBeastsPartyUIClientSubsystem::CloseOwnedPages()
{
    auto Pending=MoveTemp(PendingModels);PendingModels.Reset();
    auto OwnedBindings=MoveTemp(Bindings);Bindings.Reset();
    if(DispatchingModel)DispatchingModel->DisconnectPlayer();
    DispatchingModel=nullptr;OpenedDuringDispatch.Reset();FailedDuringDispatch.Reset();
    for(const auto& Pair:Pending){if(UI)UI->CancelOpen(Pair.Key);if(Pair.Value)Pair.Value->DisconnectPlayer();}
    for(const auto& Binding:OwnedBindings)if(Binding)Binding->CloseOwnedPage();
}
void UDivineBeastsPartyUIClientSubsystem::PlayerControllerChanged(APlayerController* Controller)
{
    Super::PlayerControllerChanged(Controller);CloseOwnedPages();
    ContextPlayer.Empty();ContextParty.Empty();ContextRevision=-1;
    ProjectPartyNotifications();
}
bool UDivineBeastsPartyUIClientSubsystem::OpenPartyPage(FName PageId)
{
    if(bClosing||!UI||!GetLocalPlayer()||!GetLocalPlayer()->GetPlayerController(GetWorld()))return false;
    if(PageId.IsNone())PageId=TEXT("UI.Party.Panel");
    if(!Definitions.ContainsByPredicate([&](const auto& Definition){return Definition&&Definition->ScreenId==PageId;}))return false;
    // 只保留一个在途公共页面请求；关闭窗口不是业务取消，也不改变队伍状态。
    if(!PendingModels.IsEmpty()||DispatchingModel)return false;
    const TStrongObjectPtr<UDivineBeastsPartyViewModel> KeepModel(NewObject<UDivineBeastsPartyViewModel>(this));
    auto* Model=KeepModel.Get(); Model->ConnectPlayer(GetLocalPlayer());
    Model->SetTargetContext(ContextPlayer,ContextParty,ContextRevision);
    DispatchingModel=Model;OpenedDuringDispatch.Reset();FailedDuringDispatch.Reset();
    const auto Request=UI->OpenScreenAsync(PageId,Model);
    const bool bOpened=OpenedDuringDispatch.Contains(Request.RequestId);
    const bool bFailed=FailedDuringDispatch.Contains(Request.RequestId);
    const bool bStillCurrent=DispatchingModel==Model&&!bClosing;
    DispatchingModel=nullptr;OpenedDuringDispatch.Reset();FailedDuringDispatch.Reset();
    if(!bStillCurrent||bFailed){if(UI&&!bOpened)UI->CancelOpen(Request.RequestId);Model->DisconnectPlayer();return false;}
    if(bOpened)return true;
    if(!Request.RequestId.IsValid()){Model->DisconnectPlayer();return false;}
    PendingModels.Add(Request.RequestId,Model);return true;
}
bool UDivineBeastsPartyUIClientSubsystem::OpenPlayerContext(const FString& PlayerId,const FString& PartyId,int64 Revision)
{
    if(PlayerId.IsEmpty())return false;ContextPlayer=PlayerId;ContextParty=PartyId;ContextRevision=Revision;
    return OpenPartyPage(TEXT("UI.Party.Context"));
}
void UDivineBeastsPartyUIClientSubsystem::ScreenOpened(FGuid RequestId,FName,UGamePlatformUIScreen* Screen)
{
    auto* Found=PendingModels.Find(RequestId);
    auto* Model=Found?Found->Get():(Screen&&Screen->GetViewModel()==DispatchingModel?DispatchingModel.Get():nullptr);
    if(!Model)return;
    if(Found)PendingModels.Remove(RequestId);else OpenedDuringDispatch.Add(RequestId);
    if(bClosing||!Screen||Screen->GetOwningLocalPlayer()!=GetLocalPlayer()||Screen->GetViewModel()!=Model){if(Model)Model->DisconnectPlayer();return;}
    const auto* Definition=Definitions.FindByPredicate([Screen](const auto& Item){return Item&&Item->ScreenId==Screen->GetScreenId();});
    auto* Root=UI?UI->GetRootLayout():nullptr;
    auto* Stack=Root&&Definition?Root->GetActivatableStack((*Definition)->Layer):nullptr;
    if(!Stack||!UI->IsScreenOwnedByStack(Screen,Stack))
    {FailedDuringDispatch.Add(RequestId);LastError=FText::FromString(TEXT("页面已离开原层栈，未建立业务绑定"));Model->DisconnectPlayer();return;}
    Bindings.RemoveAll([Screen](const auto& Old)
    {if(!Old)return true;if(!Old->IsBound()||Old->GetBoundScreen()==Screen){Old->Unbind();return true;}return false;});
    auto* Binding=NewObject<UDivineBeastsPartyScreenBinding>(this);Bindings.Add(Binding);Binding->Bind(Screen,Model,this,Stack);
}
void UDivineBeastsPartyUIClientSubsystem::ScreenClosed(FName)
{
    Bindings.RemoveAll([this](const auto& Binding)
    {
        if(!Binding)return true;
        if(!Binding->IsBound()||!UI||!UI->IsScreenOwnedByStack(Binding->GetBoundScreen(),Binding->GetOriginalStack()))
        {Binding->Unbind();return true;}
        return false;
    });
}
void UDivineBeastsPartyUIClientSubsystem::ScreenFailed(FGuid RequestId,FName,FText)
{
    auto* Found=PendingModels.Find(RequestId);if(!Found){if(DispatchingModel)FailedDuringDispatch.Add(RequestId);return;}
    if(*Found)(*Found)->DisconnectPlayer();PendingModels.Remove(RequestId);
    LastError=FText::FromString(TEXT("小队页面资源不可用，请检查当前客户端内容"));
}
FGuid UDivineBeastsPartyUIClientSubsystem::RegisterActivity(UObject* Owner,FDivineBeastsPartyActivityAction Action)
{
    if(bClosing||!IsValid(Owner)||Owner->GetTypedOuter<ULocalPlayer>()!=GetLocalPlayer()||!Action||ActivityOwner.IsValid())return {};
    ActivityOwner=Owner;ActivityAction=MoveTemp(Action);ActivityHandle=FGuid::NewGuid();return ActivityHandle;
}
bool UDivineBeastsPartyUIClientSubsystem::UnregisterActivity(FGuid Handle)
{
    if(!Handle.IsValid()||Handle!=ActivityHandle)return false;
    ActivityOwner.Reset();ActivityAction={};ActivityHandle.Invalidate();return true;
}
bool UDivineBeastsPartyUIClientSubsystem::HasActivity() const{return !bClosing&&ActivityOwner.IsValid()&&static_cast<bool>(ActivityAction);}
bool UDivineBeastsPartyUIClientSubsystem::OpenActivity(const FString& PartyId)
{
    if(!HasActivity())return false;
    // 回调可能同步注销自己；复制执行闭包，避免销毁正在执行的TFunction存储。
    auto Action=ActivityAction;return Action(PartyId);
}

void UDivineBeastsPartyUIClientSubsystem::PartyObservationChanged(const FGamePlatformPartyResult&){ProjectPartyNotifications();}
void UDivineBeastsPartyUIClientSubsystem::FlowStateChanged(const FDivineBeastsFlowViewState& State)
{
    if(State.CurrentStep!=FDivineBeastsFlowNodes::InWorld()){CloseOwnedPages();return;}
    ProjectPartyNotifications();
}
void UDivineBeastsPartyUIClientSubsystem::ProjectPartyNotifications()
{
    if(bClosing||bProjectingNotifications)return;
    TGuardValue<bool> Guard(bProjectingNotifications,true);
    auto* Source=PartySource.Get();auto* Flow=ApplicationFlow.Get();if(!Source||!Flow)return;
    const FGuid Auth=Source->GetAuthenticationGeneration();
    if(NotificationAuthGeneration!=Auth){NotificationAuthGeneration=Auth;SeenInvitationIds.Reset();SeenReadyCheckId.Empty();CloseOwnedPages();}
    const auto& Snapshot=Source->GetSnapshot();
    if(!Snapshot.bSuccess||!Auth.IsValid()||Flow->GetViewState().CurrentStep!=FDivineBeastsFlowNodes::InWorld())return;
    if(Snapshot.Snapshot.ReadyCheckId.IsEmpty())
    {
        auto Copy=Bindings;
        for(const auto& Binding:Copy)if(Binding&&Binding->IsBound()&&Binding->GetBoundScreen()->GetScreenId()==TEXT("UI.Party.Ready"))Binding->CloseOwnedPage();
    }
    else if(SeenReadyCheckId!=Snapshot.Snapshot.ReadyCheckId)
    {
        const FString Ready=Snapshot.Snapshot.ReadyCheckId;
        if(OpenPartyPage(TEXT("UI.Party.Ready")))SeenReadyCheckId=Ready;
        return;
    }
    TArray<FString> NewIds;
    for(const auto& Invite:Snapshot.Invitations)if(!SeenInvitationIds.Contains(Invite.PartyId+TEXT("/")+Invite.RequestId))NewIds.Add(Invite.PartyId+TEXT("/")+Invite.RequestId);
    if(!NewIds.IsEmpty()&&!Snapshot.Snapshot.bRosterLocked)
    {
        bool bAlreadyOpen=false;for(const auto& Binding:Bindings)if(Binding&&Binding->IsBound()&&Binding->GetBoundScreen()->GetScreenId()==TEXT("UI.Party.Invitation")){bAlreadyOpen=true;break;}
        if(bAlreadyOpen||OpenPartyPage(TEXT("UI.Party.Invitation")))for(const auto& Id:NewIds)SeenInvitationIds.Add(Id);
    }
}
