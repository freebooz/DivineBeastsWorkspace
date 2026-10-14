// SSLM演示详情/控制/报告的原生呈现桥；三份Monolith子页借用Hub的VM，不另建页面代次、目录服务或运行执行器。
// 数据与世界资源归Catalog/World，子页只拥有参数编辑副本与待审对象。游戏线程事件更新，销毁精确解绑。
#include "UI/DivineBeastsDemoHubWidget.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "UI/DivineBeastsDemoParameterRow.h"
#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/NamedSlot.h"
#include "Engine/GameInstance.h"
#include "Review/DivineBeastsDemoReviewBridge.h"
#include "UObject/StrongObjectPtr.h"

void UDivineBeastsDemoHubWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // AddDynamic宏要求可展开函数标识，因此逐项登记；不存在控件的旧兼容页无副作用。
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelLaunchButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelLaunch);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelExecuteButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelExecute);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelPauseButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelPause);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelResumeButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelResume);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelStopButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelStop);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelResetButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelReset);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelEvidenceButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelEvidence);
    RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::NativeDestruct()
{
    BindPresentationModel(nullptr);
    for(const TCHAR* Name:{TEXT("PanelLaunchButton"),TEXT("PanelExecuteButton"),TEXT("PanelPauseButton"),TEXT("PanelResumeButton"),TEXT("PanelStopButton"),TEXT("PanelResetButton"),TEXT("PanelEvidenceButton")})
        if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))B->OnClicked.RemoveAll(this);
    Super::NativeDestruct();
}
bool UDivineBeastsDemoHubWidget::HasCurrentPresentationModel() const
{
    return PresentationModel.IsValid()&&PresentationModel->IsPageActive()&&
        PresentationModel->GetPageGeneration()==PresentationPageGeneration;
}
void UDivineBeastsDemoHubWidget::BindPresentationModel(UDivineBeastsDemoHubViewModel* InModel)
{
    const int32 Generation=InModel?InModel->GetPageGeneration():0;
    if(PresentationModel.Get()==InModel&&PresentationPageGeneration==Generation) { RefreshPanelPresentation(); return; }
    if(auto* Previous=PresentationModel.Get())Previous->OnViewStateChanged.RemoveDynamic(this,&ThisClass::PanelModelChanged);
    PresentationModel=InModel; PresentationPageGeneration=Generation;
    PanelParameterScenario=NAME_None; PanelParameterItems.Reset(); PanelPendingReports.Reset();
    PanelEvidencePath.Reset(); PanelEvidenceRunId.Invalidate(); PanelCommandFeedback=FText();
    if(auto* Slot=Cast<UNamedSlot>(GetWidgetFromName(TEXT("PanelSampleSlot"))))Slot->SetContent(nullptr);
    if(InModel)InModel->OnViewStateChanged.AddUniqueDynamic(this,&ThisClass::PanelModelChanged);
    RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::PanelModelChanged(int32,int32 Generation)
{ if(Generation==PresentationPageGeneration)RefreshPanelPresentation(); }
void UDivineBeastsDemoHubWidget::RefreshPanelPresentation()
{
    const bool bCurrent=HasCurrentPresentationModel();
    const auto Snapshot=bCurrent?PresentationModel->GetMenuSnapshot():FDivineBeastsDemoMenuSnapshot();
    const auto Text=[this](const TCHAR* Name,const FText& Value)
    {if(auto* Label=Cast<UTextBlock>(GetWidgetFromName(Name)))Label->SetText(Value);};
    Text(TEXT("PanelDetailsText"),bCurrent?Snapshot.Details:FText::FromString(TEXT("演示页面未激活，等待Hub绑定当前只读模型。")));
    const auto* Selected=Snapshot.Scenarios.FindByPredicate([&](const auto& Entry){return Entry.Scenario.ScenarioId==Snapshot.SelectedScenarioId;});
    auto* Parameters=Cast<UListView>(GetWidgetFromName(TEXT("PanelParameterList")));
    if(Parameters&&PanelParameterScenario!=Snapshot.SelectedScenarioId)
    {
        PanelParameterScenario=Snapshot.SelectedScenarioId; PanelParameterItems.Reset(); TArray<UObject*> Values;
        if(Selected)for(const auto& Definition:Selected->Scenario.Parameters)
        {auto* Item=NewObject<UDivineBeastsDemoParameterItem>(this);Item->Definition=Definition;Item->Value=Definition.DefaultValue;PanelParameterItems.Add(Item);Values.Add(Item);}
        Parameters->SetListItems(Values);
    }
    if(!bCurrent&&Parameters)Parameters->ClearListItems();
    const bool bRunning=bCurrent&&Selected&&Snapshot.Run.ScenarioId==Selected->Scenario.ScenarioId&&
        Snapshot.Run.State==EDivineBeastsDemoRunState::Running&&!Snapshot.Run.bOperationPending&&!bPanelCommandDispatching;
    const auto Enable=[this](const TCHAR* Name,bool bEnabled)
    {if(auto* Button=Cast<UButton>(GetWidgetFromName(Name)))Button->SetIsEnabled(bEnabled);};
    Enable(TEXT("PanelLaunchButton"),bCurrent&&Selected&&Selected->Preflight.bCanStart&&!bPanelCommandDispatching&&
        Snapshot.Run.State!=EDivineBeastsDemoRunState::Preparing&&Snapshot.Run.State!=EDivineBeastsDemoRunState::Running&&Snapshot.Run.State!=EDivineBeastsDemoRunState::Stopping);
    Enable(TEXT("PanelExecuteButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Execute")));
    Enable(TEXT("PanelPauseButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Pause")));
    Enable(TEXT("PanelResumeButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Resume")));
    Enable(TEXT("PanelResetButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Reset")));
    Enable(TEXT("PanelStopButton"),bCurrent&&!bPanelCommandDispatching&&(Snapshot.Run.State==EDivineBeastsDemoRunState::Preparing||Snapshot.Run.State==EDivineBeastsDemoRunState::Running));
    Enable(TEXT("PanelEvidenceButton"),bCurrent&&Snapshot.Run.Token.IsValid()&&!bPanelCommandDispatching&&!Snapshot.Run.bOperationPending&&
        (Snapshot.Run.State==EDivineBeastsDemoRunState::Completed||Snapshot.Run.State==EDivineBeastsDemoRunState::Failed||Snapshot.Run.State==EDivineBeastsDemoRunState::Cancelled));
    if(auto* Slot=Cast<UNamedSlot>(GetWidgetFromName(TEXT("PanelSampleSlot"))))
    {auto* Sample=bCurrent?PresentationModel->GetSampleWidget():nullptr;if(Slot->GetContent()!=Sample)Slot->SetContent(Sample);}
    Text(TEXT("PanelStatusText"),!PanelCommandFeedback.IsEmpty()?PanelCommandFeedback:FText::FromString(Selected?
        Selected->Preflight.bCanStart?TEXT("预检已通过；尚不代表执行或人工验收成功。") : Selected->Preflight.Result.Message:
        bCurrent?Snapshot.CatalogResult.Message:TEXT("当前没有活动演示模型。")));
    FString Report=FString::Printf(TEXT("运行身份：%s\n案例：%s\n状态：%s\n已完成操作：%lld\n操作结果：%s\n清理结果：%s\n终态结果：%s\n\n人工核验：未核验；本页面不能自动签收。"),
        *Snapshot.Run.Token.RunId.ToString(),*Snapshot.Run.ScenarioId.ToString(),*StaticEnum<EDivineBeastsDemoRunState>()->GetNameStringByValue(static_cast<int64>(Snapshot.Run.State)),
        Snapshot.Run.CompletedOperations,*Snapshot.Run.OperationResult.Message,*Snapshot.Run.CleanupResult.Message,*Snapshot.Run.TerminalResult.Message);
    Text(TEXT("PanelReportText"),FText::FromString(Report));
    Text(TEXT("PanelEvidencePathText"),FText::FromString(!PanelEvidencePath.IsEmpty()&&PanelEvidenceRunId==Snapshot.Run.Token.RunId?
        TEXT("本次真实证据索引：")+PanelEvidencePath:TEXT("本次运行尚未生成证据索引；历史路径不作为当前运行证据。")));
}
void UDivineBeastsDemoHubWidget::SubmitPanelOperation(FName Operation)
{
    if(!HasCurrentPresentationModel()||bPanelCommandDispatching)return;
    TArray<FDivineBeastsDemoParameterValue> Values;
    if(Operation!=TEXT("Stop"))for(const auto& Item:PanelParameterItems)
    {if(!Item->bValidInput){PanelCommandFeedback=FText::FromString(TEXT("请先修正类型或范围错误的参数。"));RefreshPanelPresentation();return;}Values.Add(Item->Value);}
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(PresentationModel.Get());
    bool bAccepted=false;{TGuardValue<bool> Guard(bPanelCommandDispatching,true);bAccepted=Model->RequestOperation(Operation,Values);}
    if(!HasCurrentPresentationModel()||PresentationModel.Get()!=Model.Get())return;
    PanelCommandFeedback=FText::FromString(bAccepted?TEXT("命令已受理，执行结果以运行事件为准。") : TEXT("命令未受理：当前案例、能力、参数或忙碌状态不允许。"));RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::PanelExecute(){SubmitPanelOperation(TEXT("Execute"));}
void UDivineBeastsDemoHubWidget::PanelPause(){SubmitPanelOperation(TEXT("Pause"));}
void UDivineBeastsDemoHubWidget::PanelResume(){SubmitPanelOperation(TEXT("Resume"));}
void UDivineBeastsDemoHubWidget::PanelStop(){SubmitPanelOperation(TEXT("Stop"));}
void UDivineBeastsDemoHubWidget::PanelReset(){SubmitPanelOperation(TEXT("Reset"));}
void UDivineBeastsDemoHubWidget::PanelLaunch()
{
    if(!HasCurrentPresentationModel()||bPanelCommandDispatching)return;
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(PresentationModel.Get());
    bool bAccepted=false;{TGuardValue<bool> Guard(bPanelCommandDispatching,true);bAccepted=Model->RequestScenario(Model->GetMenuSnapshot().SelectedScenarioId);}
    if(!HasCurrentPresentationModel()||PresentationModel.Get()!=Model.Get())return;
    PanelCommandFeedback=FText::FromString(bAccepted?TEXT("进入请求已受理，等待案例准备结果。") : TEXT("进入请求未受理，请检查详情中的预检原因。"));RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::PanelEvidence()
{
    if(!HasCurrentPresentationModel()||bPanelCommandDispatching)return;
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Model(PresentationModel.Get());
    const auto Before=Model->GetMenuSnapshot();FString Error,Path;
    {TGuardValue<bool> Guard(bPanelCommandDispatching,true);Path=Model->RequestEvidence(Error);}
    if(!HasCurrentPresentationModel()||PresentationModel.Get()!=Model.Get()||!(Before.Run.Token==Model->GetMenuSnapshot().Run.Token))return;
    if(!Path.IsEmpty())
    {
        PanelEvidencePath=Path;PanelEvidenceRunId=Before.Run.Token.RunId;
#if WITH_EDITOR
        if(auto* Instance=GetGameInstance())if(auto* Catalog=Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>())
            DivineBeasts::Demo::CreatePendingReviews(*Catalog,Before.Run.ScenarioId,Before.Run.Token.RunId,Path,PanelPendingReports,Error);
#endif
    }
    PanelCommandFeedback=FText::FromString(!Error.IsEmpty()?Error:Path.IsEmpty()?TEXT("没有可导出的真实运行证据。") : TEXT("已生成真实待审索引；未填写审核人或人工通过结论。"));RefreshPanelPresentation();
}
