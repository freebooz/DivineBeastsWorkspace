#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Social/DivineBeastsPartyUIClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"

FDivineBeastsUIWorldProjection UDivineBeastsWorldHUDBase::GetWorldProjection() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM ? VM->GetStateRef().World : FDivineBeastsUIWorldProjection();
}
bool UDivineBeastsWorldHUDBase::HasWorldView() const
{
    const auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return VM && !VM->GetStateRef().World.WorldId.IsNone();
}

void UDivineBeastsWorldHUDBase::BindUIEvents()
{
    Super::BindUIEvents();
    bWorldControlsBound=true;++WorldControlGeneration;
    UtilityUIManager=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UGamePlatformUIManagerSubsystem>():nullptr;
    if(auto* Manager=UtilityUIManager.Get())
    {Manager->OnScreenOpened.AddUniqueDynamic(this,&ThisClass::UtilityOpened);Manager->OnScreenOpenFailed.AddUniqueDynamic(this,&ThisClass::UtilityFailed);}
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("JournalButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenJournal);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("PartyButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenParty);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("TrainingResetButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::ResetTraining);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("QuestLogButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenQuestLog);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("SocialButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenSocial);
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("NoticesButton")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::OpenNotices);
}
void UDivineBeastsWorldHUDBase::UnbindUIEvents()
{
    bWorldControlsBound=false;++WorldControlGeneration;
    if(auto* Manager=UtilityUIManager.Get())
    {Manager->OnScreenOpened.RemoveDynamic(this,&ThisClass::UtilityOpened);Manager->OnScreenOpenFailed.RemoveDynamic(this,&ThisClass::UtilityFailed);}
    UtilityUIManager.Reset();DispatchingUtilityScreen=NAME_None;SynchronousUtilityCompletion.Invalidate();
    for (const TCHAR* Name:{TEXT("JournalButton"),TEXT("PartyButton"),TEXT("TrainingResetButton"),TEXT("QuestLogButton"),TEXT("SocialButton"),TEXT("NoticesButton")})
        if (auto* Button=Cast<UButton>(GetWidgetFromName(Name))) Button->OnClicked.RemoveAll(this);
    ClearWorldCommandContext(); Super::UnbindUIEvents();
}
void UDivineBeastsWorldHUDBase::RefreshInitialState() { Super::RefreshInitialState(); RefreshWorldPresentation(); }
void UDivineBeastsWorldHUDBase::OnViewModelStateChanged(int32 Revision,int32 Generation)
{ Super::OnViewModelStateChanged(Revision,Generation); RefreshWorldPresentation(); }
void UDivineBeastsWorldHUDBase::OnAdaptiveContextChanged(const FGamePlatformUIAdaptiveContext& Context)
{ Super::OnAdaptiveContextChanged(Context); RefreshWorldPresentation(); }
void UDivineBeastsWorldHUDBase::RefreshWorldPresentation()
{
    const auto* Model=Cast<UDivineBeastsUIViewModel>(GetViewModel());
    const auto Projection=GetWorldProjection();
    const bool bHasWorld=HasWorldView();
    const auto Text=[this](const TCHAR* Name,const FString& Value)
    { if (auto* Label=Cast<UTextBlock>(GetWidgetFromName(Name))) Label->SetText(FText::FromString(Value)); };
    Text(TEXT("WorldStatusText"),bHasWorld?FString::Printf(TEXT("世界 %s · 区域 %s"),*Projection.WorldId.ToString(),*Projection.RegionId.ToString()):TEXT("等待当前世界的授权状态"));
    const bool bHealthKnown=bHasWorld&&FMath::IsFinite(Projection.Health)&&FMath::IsFinite(Projection.MaxHealth)&&Projection.MaxHealth>0.0f;
    Text(TEXT("PlayerStatusText"),bHealthKnown?FString::Printf(TEXT("生命 %.0f / %.0f"),Projection.Health,Projection.MaxHealth):TEXT("生命状态尚未提供"));
    if (auto* Bar=Cast<UProgressBar>(GetWidgetFromName(TEXT("HealthProgress"))))
    { Bar->SetVisibility(bHealthKnown?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed); Bar->SetPercent(bHealthKnown?FMath::Clamp(Projection.Health/Projection.MaxHealth,0.0f,1.0f):0.0f); }
    FString Objectives;
    for (FName Id:Projection.QuestObjectiveIds) Objectives+=Id.ToString()+TEXT("\n");
    Text(TEXT("GuidanceText"),Objectives.IsEmpty()?TEXT("当前没有已发布的任务目标；教学步骤由世界服务提供。") : Objectives);
    const auto Device=GetAdaptiveContext().InputDeviceClass;
    Text(TEXT("InputHintText"),Device==EGamePlatformUIInputDeviceClass::Gamepad?TEXT("使用手柄焦点导航选择操作；按当前输入设置移动和选择目标。"):
        Device==EGamePlatformUIInputDeviceClass::Touch?TEXT("触摸界面按钮打开手册和小队；移动与选中使用已配置的触控操作。"):
        TEXT("鼠标选择界面操作；移动、目标与快捷键以系统设置中的输入绑定为准。"));
    const bool bCanReset=Model&&Model->IsPageActive()&&GetHUDKind()==EDivineBeastsHUDKind::Training&&
        !Model->GetStateRef().bBusy&&Model->GetStateRef().AllowedCommands.Contains(TEXT("TrainingReset"));
    if (auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("TrainingResetButton")))) Button->SetIsEnabled(bCanReset);
    Text(TEXT("CommandFeedbackText"),!WorldCommandFeedback.IsEmpty()?WorldCommandFeedback.ToString():
        GetHUDKind()==EDivineBeastsHUDKind::Training&&!bCanReset?TEXT("当前训练服务未授权重置；不会直接修改生命、冷却或服务器状态。") : TEXT(""));
}
void UDivineBeastsWorldHUDBase::OpenJournal()
{
    if(!CanSubmitWorldCommand())return;
    auto* Owner=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsUIClientSubsystem>():nullptr;
    if (!Owner||!Owner->OpenWorldJournal(TEXT("Character"))) WorldCommandFeedback=NSLOCTEXT("SSLMUI","JournalNotAccepted","当前不能打开角色手册，请检查世界与菜单状态。");
    else WorldCommandFeedback=FText(); RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::OpenParty()
{
    if(!CanSubmitWorldCommand())return;
    auto* Owner=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsPartyUIClientSubsystem>():nullptr;
    WorldCommandFeedback=Owner&&Owner->OpenPartyPage()?FText():NSLOCTEXT("SSLMUI","HUDPartyNotAccepted","小队页面未受理，未改变任何队伍。"); RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::ResetTraining()
{
    if(!CanSubmitWorldCommand())return;
    auto* Model=Cast<UDivineBeastsUIViewModel>(GetViewModel());
    const bool bAllowed=Model&&Model->IsPageActive()&&GetHUDKind()==EDivineBeastsHUDKind::Training&&
        !Model->GetStateRef().bBusy&&Model->GetStateRef().AllowedCommands.Contains(TEXT("TrainingReset"));
    const FGuid Request=bAllowed?Model->RequestTrainingReset():FGuid();
    WorldCommandFeedback=FText::FromString(Request.IsValid()?TEXT("重置请求已提交，等待真实服务结果。") : TEXT("训练重置未受理；未修改服务器状态。")); RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::OpenUtility(FName ScreenId)
{
    if(!CanSubmitWorldCommand()||!DispatchingUtilityScreen.IsNone())return;
    const uint64 Generation=WorldControlGeneration;
    auto* Owner=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsUIClientSubsystem>():nullptr;
    // 每个已知页面最多一个在途身份；终结后移除，反复点击不会无界累积请求或重复保留加载。
    if(const auto* Previous=OwnedUtilityRequests.Find(ScreenId)){const FGuid Id=*Previous;OwnedUtilityRequests.Remove(ScreenId);if(Owner)Owner->CancelScreenOpen(Id);}
    DispatchingUtilityScreen=ScreenId;SynchronousUtilityCompletion.Invalidate();
    const auto Request=Owner?Owner->OpenWorldUtilityScreen(ScreenId):FGamePlatformUIAsyncRequest();
    DispatchingUtilityScreen=NAME_None;
    if(Generation!=WorldControlGeneration||!CanSubmitWorldCommand()){if(Owner&&Request.IsValid())Owner->CancelScreenOpen(Request.RequestId);return;}
    if(Request.IsValid()&&SynchronousUtilityCompletion==Request.RequestId){RefreshWorldPresentation();return;}
    if(Request.IsValid())OwnedUtilityRequests.Add(ScreenId,Request.RequestId);
    WorldCommandFeedback=FText::FromString(Request.IsValid()?TEXT("页面打开请求已提交；显示结果由平台异步事件确认。") : TEXT("当前世界或页面服务不允许打开此界面。"));RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::UtilityOpened(FGuid RequestId,FName ScreenId,UGamePlatformUIScreen*)
{
    const auto* Pending=OwnedUtilityRequests.Find(ScreenId);
    if(!bWorldControlsBound||(!Pending&&DispatchingUtilityScreen!=ScreenId)||(Pending&&*Pending!=RequestId))return;
    if(DispatchingUtilityScreen==ScreenId)SynchronousUtilityCompletion=RequestId;
    OwnedUtilityRequests.Remove(ScreenId);WorldCommandFeedback=FText();RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::UtilityFailed(FGuid RequestId,FName ScreenId,FText Reason)
{
    const auto* Pending=OwnedUtilityRequests.Find(ScreenId);
    if(!bWorldControlsBound||(!Pending&&DispatchingUtilityScreen!=ScreenId)||(Pending&&*Pending!=RequestId))return;
    if(DispatchingUtilityScreen==ScreenId)SynchronousUtilityCompletion=RequestId;
    OwnedUtilityRequests.Remove(ScreenId);
    WorldCommandFeedback=Reason.IsEmpty()?NSLOCTEXT("SSLMUI","UtilityOpenFailed","页面加载失败，请稍后重试。"):Reason;RefreshWorldPresentation();
}
void UDivineBeastsWorldHUDBase::OpenQuestLog(){OpenUtility(TEXT("UI.Screen.Quest"));}
void UDivineBeastsWorldHUDBase::OpenSocial(){OpenUtility(TEXT("UI.Screen.Social"));}
void UDivineBeastsWorldHUDBase::OpenNotices(){OpenUtility(TEXT("UI.Screen.LiveOps"));}

bool UDivineBeastsWorldHUDBase::CanSubmitWorldCommand() const
{
    const auto* Model=Cast<UDivineBeastsUIViewModel>(GetViewModel());
    return bWorldControlsBound&&IsVisible()&&Model&&Model->IsPageActive();
}
void UDivineBeastsWorldHUDBase::ClearWorldCommandContext()
{
    ++WorldControlGeneration;DispatchingUtilityScreen=NAME_None;SynchronousUtilityCompletion.Invalidate();
    const auto Requests=MoveTemp(OwnedUtilityRequests);OwnedUtilityRequests.Reset();WorldCommandFeedback=FText();
    if(auto* Owner=GetOwningLocalPlayer()?GetOwningLocalPlayer()->GetSubsystem<UDivineBeastsUIClientSubsystem>():nullptr)
        for(const auto& Request:Requests)Owner->CancelScreenOpen(Request.Value);
}
