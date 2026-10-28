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
#include "Misc/ScopeExit.h"
#include "Async/Async.h"

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
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PanelRunSelectedOperationButton"))))B->OnClicked.AddUniqueDynamic(this,&ThisClass::PanelRunSelectedOperation);
    RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::NativeDestruct()
{
    BindPresentationModel(nullptr);
    for(const TCHAR* Name:{TEXT("PanelLaunchButton"),TEXT("PanelExecuteButton"),TEXT("PanelPauseButton"),TEXT("PanelResumeButton"),TEXT("PanelStopButton"),TEXT("PanelResetButton"),TEXT("PanelEvidenceButton"),TEXT("PanelRunSelectedOperationButton")})
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
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> KeepModel(InModel);
    const int32 Generation=InModel?InModel->GetPageGeneration():0;
    if(PresentationModel.Get()==InModel&&PresentationPageGeneration==Generation) { RefreshPanelPresentation(); return; }
    if(auto* Previous=PresentationModel.Get())Previous->OnViewStateChanged.RemoveDynamic(this,&ThisClass::PanelModelChanged);
    PresentationModel=InModel; PresentationPageGeneration=Generation;
    PanelParameterScenario=NAME_None; PanelParameterFingerprint.Reset(); PanelParameterItems.Reset(); PanelPendingReports.Reset();
    PanelEvidencePath.Reset(); PanelEvidenceRunId.Invalidate(); PanelCommandFeedback=FText();
    PanelOperationIds.Reset(); PanelOperationsScenario=NAME_None; PanelOperationsFingerprint.Reset();
    if(auto* SampleSlot=Cast<UNamedSlot>(GetWidgetFromName(TEXT("PanelSampleSlot"))))SampleSlot->SetContent(nullptr);
    if(PresentationModel.Get()!=InModel||PresentationPageGeneration!=Generation)return;
    if(InModel)InModel->OnViewStateChanged.AddUniqueDynamic(this,&ThisClass::PanelModelChanged);
    RefreshPanelPresentation();
}
void UDivineBeastsDemoHubWidget::PanelModelChanged(int32,int32 Generation)
{ if(Generation==PresentationPageGeneration)RefreshPanelPresentation(); }
void UDivineBeastsDemoHubWidget::RefreshPanelPresentation()
{
    const bool bCurrent=HasCurrentPresentationModel();
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Expected(PresentationModel.Get());
    const int32 Generation=PresentationPageGeneration;
    const int32 Revision=Expected.IsValid()?Expected->GetRevision():0;
    const auto StillOwnsRefresh=[this,&Expected,Generation,Revision,bCurrent]()
    {return PresentationModel.Get()==Expected.Get()&&PresentationPageGeneration==Generation&&HasCurrentPresentationModel()==bCurrent&&
        (!Expected.IsValid()||Expected->GetRevision()==Revision);};
    const auto Snapshot=bCurrent?Expected->GetMenuSnapshot():FDivineBeastsDemoMenuSnapshot();
    const auto Text=[this,&StillOwnsRefresh](const TCHAR* Name,const FText& Value)
    {if(StillOwnsRefresh())if(auto* Label=Cast<UTextBlock>(GetWidgetFromName(Name)))Label->SetText(Value);};
    Text(TEXT("PanelDetailsText"),bCurrent?Snapshot.Details:FText::FromString(TEXT("演示页面未激活，等待Hub绑定当前只读模型。")));
    if(!StillOwnsRefresh())return;
    const auto* Selected=Snapshot.Scenarios.FindByPredicate([&](const auto& Entry){return Entry.Scenario.ScenarioId==Snapshot.SelectedScenarioId;});
    auto* Parameters=Cast<UListView>(GetWidgetFromName(TEXT("PanelParameterList")));
    if(Parameters&&(PanelParameterScenario!=Snapshot.SelectedScenarioId||PanelParameterFingerprint!=Snapshot.SourceFingerprint))
    {
        PanelParameterScenario=Snapshot.SelectedScenarioId; PanelParameterFingerprint=Snapshot.SourceFingerprint;
        PanelCommandFeedback=FText(); PanelParameterItems.Reset(); TArray<UObject*> Values;
        if(Selected)for(const auto& Definition:Selected->Scenario.Parameters)
        {auto* Item=NewObject<UDivineBeastsDemoParameterItem>(this);Item->Definition=Definition;Item->Value=Definition.DefaultValue;PanelParameterItems.Add(Item);Values.Add(Item);}
        Parameters->SetListItems(Values);if(!StillOwnsRefresh())return;
    }
    if(!bCurrent&&Parameters)Parameters->ClearListItems();
    if(!StillOwnsRefresh())return;
    const bool bRunning=bCurrent&&Selected&&Snapshot.Run.ScenarioId==Selected->Scenario.ScenarioId&&
        Snapshot.Run.State==EDivineBeastsDemoRunState::Running&&!Snapshot.Run.bOperationPending&&!bPanelCommandDispatching;
    RefreshPanelOperationChoices(Selected?&Selected->Scenario:nullptr,bRunning);
    if(!StillOwnsRefresh())return;
    const auto Enable=[this,&StillOwnsRefresh](const TCHAR* Name,bool bEnabled)
    {if(StillOwnsRefresh())if(auto* Button=Cast<UButton>(GetWidgetFromName(Name)))Button->SetIsEnabled(bEnabled);};
    Enable(TEXT("PanelLaunchButton"),bCurrent&&Selected&&Selected->Preflight.bCanStart&&!bPanelCommandDispatching&&
        Snapshot.Run.State!=EDivineBeastsDemoRunState::Preparing&&Snapshot.Run.State!=EDivineBeastsDemoRunState::Running&&Snapshot.Run.State!=EDivineBeastsDemoRunState::Stopping);
    Enable(TEXT("PanelExecuteButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Execute")));
    Enable(TEXT("PanelPauseButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Pause")));
    Enable(TEXT("PanelResumeButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Resume")));
    Enable(TEXT("PanelResetButton"),bRunning&&Selected->Scenario.Operations.Contains(TEXT("Reset")));
    Enable(TEXT("PanelStopButton"),bCurrent&&!bPanelCommandDispatching&&(Snapshot.Run.State==EDivineBeastsDemoRunState::Preparing||Snapshot.Run.State==EDivineBeastsDemoRunState::Running||(Snapshot.Run.bCleanupPending&&Snapshot.Run.State!=EDivineBeastsDemoRunState::Stopping)));
    Enable(TEXT("PanelEvidenceButton"),bCurrent&&Snapshot.Run.Token.IsValid()&&!bPanelCommandDispatching&&!Snapshot.Run.bOperationPending&&
        (Snapshot.Run.State==EDivineBeastsDemoRunState::Completed||Snapshot.Run.State==EDivineBeastsDemoRunState::Failed||Snapshot.Run.State==EDivineBeastsDemoRunState::Cancelled));
    if(!StillOwnsRefresh())return;
    if(auto* SampleSlot=Cast<UNamedSlot>(GetWidgetFromName(TEXT("PanelSampleSlot"))))
    {auto* Sample=bCurrent?PresentationModel->GetSampleWidget():nullptr;if(SampleSlot->GetContent()!=Sample)SampleSlot->SetContent(Sample);}
    if(!StillOwnsRefresh())return;
    FString Status=(!PanelCommandFeedback.IsEmpty()?PanelCommandFeedback:FText::FromString(Selected?
        Selected->Preflight.bCanStart?TEXT("预检已通过；尚不代表执行或人工验收成功。") : Selected->Preflight.Result.Message:
        bCurrent?Snapshot.CatalogResult.Message:TEXT("当前没有活动演示模型。"))).ToString();
    if(bCurrent&&Snapshot.Run.ObservationRevision>0) Status+=TEXT("\n最近服务事件：")+Snapshot.Run.ObservationText;
    Text(TEXT("PanelStatusText"),FText::FromString(Status));
    FString Report=FString::Printf(TEXT("运行身份：%s\n案例：%s\n状态：%s\n已完成操作：%lld\n操作结果：%s\n清理结果：%s\n终态结果：%s\n\n人工核验：未核验；本页面不能自动签收。"),
        *Snapshot.Run.Token.RunId.ToString(),*Snapshot.Run.ScenarioId.ToString(),*StaticEnum<EDivineBeastsDemoRunState>()->GetNameStringByValue(static_cast<int64>(Snapshot.Run.State)),
        Snapshot.Run.CompletedOperations,*Snapshot.Run.OperationResult.Message,*Snapshot.Run.CleanupResult.Message,*Snapshot.Run.TerminalResult.Message);
    Report+=FString::Printf(TEXT("\n\n最近服务观察 #%d（不覆盖命令/清理终态）：\n%s"),Snapshot.Run.ObservationRevision,*Snapshot.Run.ObservationText);
    Text(TEXT("PanelReportText"),FText::FromString(Report));
    Text(TEXT("PanelEvidencePathText"),FText::FromString(!PanelEvidencePath.IsEmpty()&&PanelEvidenceRunId==Snapshot.Run.Token.RunId?
        TEXT("本次真实证据索引：")+PanelEvidencePath:TEXT("本次运行尚未生成证据索引；历史路径不作为当前运行证据。")));
}
void UDivineBeastsDemoHubWidget::RefreshPanelOperationChoices(const FDivineBeastsDemoScenarioDefinition* Scenario,bool bCanExecute)
{
    if(bRefreshingPanelOperationChoices){bPendingPanelOperationChoiceRefresh=true;return;}
    bRefreshingPanelOperationChoices=true;
    bool bChoicesComplete=false;
    ON_SCOPE_EXIT
    {
        bRefreshingPanelOperationChoices=false;
        if(!bChoicesComplete){PanelOperationsScenario=NAME_None;PanelOperationsFingerprint.Reset();PanelOperationIds.Reset();}
        if(bPendingPanelOperationChoiceRefresh)
        {
            bPendingPanelOperationChoiceRefresh=false;
            const TWeakObjectPtr<UDivineBeastsDemoHubWidget> Weak(this);
            // 仅同步重入后的一次收敛事件，不是Timer/Ticker/轮询；执行时读取最新活动来源。
            AsyncTask(ENamedThreads::GameThread,[Weak](){if(auto* Self=Weak.Get())if(Self->HasCurrentPresentationModel())Self->RefreshPanelPresentation();});
        }
    };
    auto* Combo=Cast<UComboBoxString>(GetWidgetFromName(TEXT("PanelOperationCombo")));
    auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("PanelRunSelectedOperationButton")));
    if(!Combo){if(Button)Button->SetIsEnabled(false);return;}
    const TStrongObjectPtr<UDivineBeastsDemoHubViewModel> Expected(HasCurrentPresentationModel()?PresentationModel.Get():nullptr);
    const auto* Model=Expected.Get();
    const int32 PageGeneration=PresentationPageGeneration;
    const int32 Revision=Model?Model->GetRevision():0;
    const auto StillOwns=[this,&Expected,PageGeneration,Revision]()
    {
        return PresentationModel.Get()==Expected.Get()&&PresentationPageGeneration==PageGeneration&&
            (!Expected.IsValid()||(HasCurrentPresentationModel()&&Expected->GetRevision()==Revision));
    };
    const FString Fingerprint=Model?Model->GetMenuSnapshot().SourceFingerprint:FString();
    const FName ScenarioId=Scenario?Scenario->ScenarioId:NAME_None;
    if(PanelOperationsScenario!=ScenarioId||PanelOperationsFingerprint!=Fingerprint||(!Scenario&&!PanelOperationIds.IsEmpty()))
    {
        const int32 OldIndex=Combo->GetSelectedIndex();
        const FName Previous=PanelOperationIds.IsValidIndex(OldIndex)?PanelOperationIds[OldIndex]:NAME_None;
        PanelOperationsScenario=ScenarioId;PanelOperationsFingerprint=Fingerprint;
        PanelOperationIds.Reset();Combo->ClearOptions();if(!StillOwns())return;
        if(Scenario)for(const FName Operation:Scenario->Operations)
        {
            if(Operation==TEXT("Execute")||Operation==TEXT("Pause")||Operation==TEXT("Resume")||Operation==TEXT("Stop")||Operation==TEXT("Reset"))continue;
            const auto* Label=Scenario->OperationLabels.Find(Operation);
            if(!Label||Label->IsEmpty())continue; // 源合同校验失败不能用英文猜测成另一个动作。
            PanelOperationIds.Add(Operation);Combo->AddOption(Label->ToString()+TEXT("  [")+Operation.ToString()+TEXT("]"));
            if(!StillOwns())return;
        }
        if(!PanelOperationIds.IsEmpty())Combo->SetSelectedIndex(PanelOperationIds.Contains(Previous)?PanelOperationIds.IndexOfByKey(Previous):0);
        if(!StillOwns())return;
    }
    const bool bHasChoices=!PanelOperationIds.IsEmpty();
    Combo->SetVisibility(bHasChoices?ESlateVisibility::Visible:ESlateVisibility::Collapsed);Combo->SetIsEnabled(bCanExecute&&bHasChoices);
    if(!StillOwns())return;
    if(Button){Button->SetVisibility(bHasChoices?ESlateVisibility::Visible:ESlateVisibility::Collapsed);Button->SetIsEnabled(bCanExecute&&bHasChoices);}
    if(!StillOwns())return;
    bChoicesComplete=true;
}
void UDivineBeastsDemoHubWidget::PanelRunSelectedOperation()
{
    if(!HasCurrentPresentationModel()||bPanelCommandDispatching)return;
    const auto Snapshot=PresentationModel->GetMenuSnapshot();
    auto* Combo=Cast<UComboBoxString>(GetWidgetFromName(TEXT("PanelOperationCombo")));
    const int32 Index=Combo?Combo->GetSelectedIndex():INDEX_NONE;
    if(PanelOperationsScenario!=Snapshot.SelectedScenarioId||PanelOperationsFingerprint!=Snapshot.SourceFingerprint||!PanelOperationIds.IsValidIndex(Index))return;
    SubmitPanelOperation(PanelOperationIds[Index]);
}
void UDivineBeastsDemoHubWidget::SubmitPanelOperation(FName Operation)
{
    if(!HasCurrentPresentationModel()||bPanelCommandDispatching)return;
    const auto Current=PresentationModel->GetMenuSnapshot();
    if(Operation!=TEXT("Stop")&&(PanelParameterScenario!=Current.SelectedScenarioId||PanelParameterFingerprint!=Current.SourceFingerprint))return;
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
