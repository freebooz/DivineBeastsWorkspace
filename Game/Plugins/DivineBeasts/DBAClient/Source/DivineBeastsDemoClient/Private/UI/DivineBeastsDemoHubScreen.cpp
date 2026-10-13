// SSLM菜单桥接：只将值项提供给Monolith虚拟化列表并转发用户命令，布局不在C++制作。
#include "UI/DivineBeastsDemoHubScreen.h"
#include "UI/DivineBeastsDemoMenuItem.h"
#include "UI/DivineBeastsDemoParameterRow.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/ListView.h"
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"

void UDivineBeastsDemoHubScreen::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (PluginSearchInput) PluginSearchInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::OnSearchChanged);
    if (LayerFilterCombo)
    {
        for (const TCHAR* Label : {TEXT("全部"), TEXT("平台层"), TEXT("MOBA通用层"), TEXT("项目层"), TEXT("项目内容包")}) LayerFilterCombo->AddOption(Label);
        LayerFilterCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnLayerChanged);
        LayerFilterCombo->SetSelectedIndex(0);
    }
    if (PluginList) PluginList->OnItemClicked().AddUObject(this, &ThisClass::OnPluginClicked);
    if (FeatureList) FeatureList->OnItemClicked().AddUObject(this, &ThisClass::OnFeatureClicked);
    if (ScenarioList) ScenarioList->OnItemClicked().AddUObject(this, &ThisClass::OnScenarioClicked);
    if (RefreshButton) RefreshButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnRefreshClicked);
    if (LaunchScenarioButton) LaunchScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnLaunchClicked);
    if (ExecuteScenarioButton) ExecuteScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnExecuteClicked);
    if (PauseScenarioButton) PauseScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnPauseClicked);
    if (ResumeScenarioButton) ResumeScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnResumeClicked);
    if (StopScenarioButton) StopScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnStopClicked);
    if (ResetDemoButton) ResetDemoButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnResetClicked);
    if (ReturnButton) ReturnButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnReturnClicked);
    if (EvidenceButton) EvidenceButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnEvidenceClicked);
}
void UDivineBeastsDemoHubScreen::BindUIEvents()
{
    Super::BindUIEvents();
}
void UDivineBeastsDemoHubScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    if (!IsActivated()) return;
    DemoModel = Cast<UDivineBeastsDemoHubViewModel>(GetViewModel());
    if (!DemoModel) { if (OperationStatusText) OperationStatusText->SetText(FText::FromString(TEXT("页面缺少演示ViewModel。"))); return; }
    auto* Instance = GetGameInstance(); DemoModel->BindCatalog(Instance ? Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr);
    if (IsActivated() && DemoModel == GetViewModel() && DemoModel->IsPageActive()) ApplySnapshot();
}
void UDivineBeastsDemoHubScreen::DetachViewModel()
{
    DemoModel = nullptr;
    if (SampleWidgetSlot) SampleWidgetSlot->SetContent(nullptr);
}
void UDivineBeastsDemoHubScreen::UnbindUIEvents() { DetachViewModel(); Super::UnbindUIEvents(); }
void UDivineBeastsDemoHubScreen::NativeDestruct() { DetachViewModel(); Super::NativeDestruct(); }
bool UDivineBeastsDemoHubScreen::NativeOnHandleBackAction() { return DemoModel && DemoModel->RequestReturn(); }
void UDivineBeastsDemoHubScreen::OnViewModelStateChanged(int32, int32 PageGeneration)
{
    // 平台先核验可见代次；此处还核验实际模型，不能向已经EndPage的旧模型发送命令。
    DemoModel = Cast<UDivineBeastsDemoHubViewModel>(GetViewModel());
    if (DemoModel && DemoModel->IsPageActive() && DemoModel->GetPageGeneration() == PageGeneration) ApplySnapshot();
}
void UDivineBeastsDemoHubScreen::OnSearchChanged(const FText& Text)
{ if (!bSynchronizing && DemoModel) { Filter.SearchText = Text.ToString(); DemoModel->SetFilter(Filter); } }
void UDivineBeastsDemoHubScreen::OnLayerChanged(FString, ESelectInfo::Type)
{
    if (bSynchronizing || !DemoModel || !LayerFilterCombo) return;
    Filter.Layer = NAME_None; Filter.PluginKind = NAME_None;
    switch (LayerFilterCombo->GetSelectedIndex())
    {
    case 1: Filter.Layer = TEXT("Platform"); break;
    case 2: Filter.Layer = TEXT("Moba"); break;
    case 3: Filter.Layer = TEXT("Project"); break;
    case 4: Filter.Layer = TEXT("Project"); Filter.PluginKind = TEXT("content_pack"); break;
    default: break;
    }
    DemoModel->SetFilter(Filter);
}
void UDivineBeastsDemoHubScreen::OnPluginClicked(UObject* Object)
{ if (!bSynchronizing && DemoModel) if (auto* Item = Cast<UDivineBeastsDemoMenuItem>(Object)) DemoModel->SelectPlugin(Item->Identity); }
void UDivineBeastsDemoHubScreen::OnFeatureClicked(UObject* Object)
{ if (!bSynchronizing && DemoModel) if (auto* Item = Cast<UDivineBeastsDemoMenuItem>(Object)) DemoModel->SelectFeature(Item->Identity); }
void UDivineBeastsDemoHubScreen::OnScenarioClicked(UObject* Object)
{ if (!bSynchronizing && DemoModel) if (auto* Item = Cast<UDivineBeastsDemoMenuItem>(Object)) DemoModel->SelectScenario(Item->Identity); }
void UDivineBeastsDemoHubScreen::ApplySnapshot()
{
    if (!DemoModel || !IsActivated() || DemoModel != GetViewModel() || !DemoModel->IsPageActive()) return;
    TGuardValue<bool> Guard(bSynchronizing, true);
    const auto Snapshot = DemoModel->GetMenuSnapshot(); MenuItems.Reset();
    const auto Populate = [this](UListView* List, const auto& Values, auto Read, FName Selected)
    {
        if (!List) return;
        TArray<UObject*> Items; UObject* Selection = nullptr;
        for (const auto& Value : Values)
        {
            auto* Item = NewObject<UDivineBeastsDemoMenuItem>(this); Read(Value, *Item);
            MenuItems.Add(Item); Items.Add(Item); if (Item->Identity == Selected) Selection = Item;
        }
        List->SetListItems(Items); if (Selection) List->SetSelectedItem(Selection);
    };
    Populate(PluginList, Snapshot.Plugins, [](const auto& P, auto& Item)
    { Item.Identity = P.PluginId; Item.Label = P.DisplayName; Item.Status = FText::FromString(P.bEnabled ? TEXT("已启用；功能范围见详情") : TEXT("已禁用；保留功能信息")); }, Snapshot.SelectedPluginId);
    Populate(FeatureList, Snapshot.Features, [](const auto& F, auto& Item)
    { Item.Identity = F.Feature.FeatureId; Item.Label = F.Feature.Title; Item.Status = FText::FromString(TEXT("实现：") + F.Feature.ImplementationStatus.ToString() + TEXT("；接入：") + F.Feature.IntegrationStatus.ToString() + TEXT("；人工未核验")); }, Snapshot.SelectedFeatureId);
    Populate(ScenarioList, Snapshot.Scenarios, [](const auto& S, auto& Item)
    { Item.Identity = S.Scenario.ScenarioId; Item.Label = S.Scenario.Title; Item.Status = FText::FromString(S.Preflight.bCanStart ? TEXT("预检通过，待运行") : S.Preflight.Result.Message); }, Snapshot.SelectedScenarioId);
    const auto* Selected = Snapshot.Scenarios.FindByPredicate([&Snapshot](const auto& S) { return S.Scenario.ScenarioId == Snapshot.SelectedScenarioId; });
    if (ParametersForScenario != Snapshot.SelectedScenarioId)
    {
        ParametersForScenario = Snapshot.SelectedScenarioId; ParameterItems.Reset(); TArray<UObject*> Values;
        if (Selected) for (const auto& Parameter : Selected->Scenario.Parameters)
        { auto* Item = NewObject<UDivineBeastsDemoParameterItem>(this); Item->Definition = Parameter; Item->Value = Parameter.DefaultValue; ParameterItems.Add(Item); Values.Add(Item); }
        if (ParameterList) ParameterList->SetListItems(Values);
    }
    if (FeatureDetailsText) FeatureDetailsText->SetText(Snapshot.Details);
    if (CoverageText) CoverageText->SetText(FText::FromString(FString::Printf(TEXT("插件%d；待审计%d；已登记功能%d；已接线%d；当前人工通过%d%s"),
        Snapshot.TotalPlugins, Snapshot.UnreviewedPlugins, Snapshot.KnownFeatures, Snapshot.WiredFeatures, Snapshot.HumanApprovedFeatures,
        Snapshot.bHasKnownDenominator ? TEXT("") : TEXT("；功能分母未知"))));
    if (OperationStatusText) OperationStatusText->SetText(FText::FromString(Snapshot.Run.TerminalResult.Status != EGamePlatformResultStatus::NotExecuted
        ? Snapshot.Run.TerminalResult.Message : (Snapshot.Run.OperationResult.Status != EGamePlatformResultStatus::NotExecuted ? Snapshot.Run.OperationResult.Message : Snapshot.CatalogResult.Message)));
    if (LaunchScenarioButton) LaunchScenarioButton->SetIsEnabled(Selected && Selected->Preflight.bCanStart && Snapshot.Run.State != EDivineBeastsDemoRunState::Stopping);
    const bool bCanOperate = Selected && Selected->Scenario.ScenarioId == Snapshot.Run.ScenarioId && Snapshot.Run.State == EDivineBeastsDemoRunState::Running && !Snapshot.Run.bOperationPending;
    if (ExecuteScenarioButton) ExecuteScenarioButton->SetIsEnabled(bCanOperate && Selected->Scenario.Operations.Contains(TEXT("Execute")));
    if (PauseScenarioButton) PauseScenarioButton->SetIsEnabled(bCanOperate && Selected->Scenario.Operations.Contains(TEXT("Pause")));
    if (ResumeScenarioButton) ResumeScenarioButton->SetIsEnabled(bCanOperate && Selected->Scenario.Operations.Contains(TEXT("Resume")));
    if (ResetDemoButton) ResetDemoButton->SetIsEnabled(bCanOperate && Selected->Scenario.Operations.Contains(TEXT("Reset")));
    if (StopScenarioButton) StopScenarioButton->SetIsEnabled(Snapshot.Run.State == EDivineBeastsDemoRunState::Preparing || Snapshot.Run.State == EDivineBeastsDemoRunState::Running);
    if (SampleWidgetSlot && SampleWidgetSlot->GetContent() != DemoModel->GetSampleWidget()) SampleWidgetSlot->SetContent(DemoModel->GetSampleWidget());
    BP_OnMenuSnapshotChanged(Snapshot);
}
void UDivineBeastsDemoHubScreen::SubmitOperation(FName Operation)
{
    if (!DemoModel) return;
    TArray<FDivineBeastsDemoParameterValue> Values;
    for (const auto& Parameter : ParameterItems)
    { if (!Parameter->bValidInput) { if (OperationStatusText) OperationStatusText->SetText(FText::FromString(TEXT("请先修正参数输入。"))); return; } Values.Add(Parameter->Value); }
    if (!DemoModel->RequestOperation(Operation, Values) && OperationStatusText) OperationStatusText->SetText(FText::FromString(TEXT("命令未受理：检查案例、能力、参数或忙碌状态。")));
}
void UDivineBeastsDemoHubScreen::OnRefreshClicked() { if (DemoModel) DemoModel->RequestRefresh(); }
void UDivineBeastsDemoHubScreen::OnLaunchClicked() { if (DemoModel) DemoModel->RequestScenario(DemoModel->GetMenuSnapshot().SelectedScenarioId); }
void UDivineBeastsDemoHubScreen::OnExecuteClicked() { SubmitOperation(TEXT("Execute")); }
void UDivineBeastsDemoHubScreen::OnPauseClicked() { SubmitOperation(TEXT("Pause")); }
void UDivineBeastsDemoHubScreen::OnResumeClicked() { SubmitOperation(TEXT("Resume")); }
void UDivineBeastsDemoHubScreen::OnStopClicked() { if (DemoModel) DemoModel->RequestOperation(TEXT("Stop"), {}); }
void UDivineBeastsDemoHubScreen::OnResetClicked() { SubmitOperation(TEXT("Reset")); }
void UDivineBeastsDemoHubScreen::OnReturnClicked() { if (DemoModel) DemoModel->RequestReturn(); }
void UDivineBeastsDemoHubScreen::OnEvidenceClicked()
{
    if (!DemoModel) return;
    FString Error; const FString Path = DemoModel->RequestEvidence(Error);
    if (!Path.IsEmpty()) BP_OnEvidenceRequested(Path);
    else if (OperationStatusText) OperationStatusText->SetText(FText::FromString(Error));
}
