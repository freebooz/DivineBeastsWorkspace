// 项目层UI适配：读取演示目录快照并转发操作，无URL、账号或业务网络请求。
#include "UI/DivineBeastsDemoHubWidget.h"
#include "Engine/GameInstance.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

UDivineBeastsDemoCatalogSubsystem* UDivineBeastsDemoHubWidget::GetCatalog() const
{
    UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr;
}

TArray<FDivineBeastsDemoPlugin> UDivineBeastsDemoHubWidget::GetDiscoveredPlugins() const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetPlugins() : TArray<FDivineBeastsDemoPlugin>();
}

TArray<FDivineBeastsDemoPlugin> UDivineBeastsDemoHubWidget::FilterDiscoveredPlugins(
    const FString& SearchText, bool bFilterLayer, EDivineBeastsDemoLayer Layer, bool bOnlyWithMaps) const
{
    // 仅对初始化/人工刷新时产生的目录快照筛选，输入变化不触发资产扫描或网络请求。
    const FString Query = SearchText.TrimStartAndEnd();
    TArray<FDivineBeastsDemoPlugin> Results;
    for (const FDivineBeastsDemoPlugin& Plugin : GetDiscoveredPlugins())
    {
        if (bFilterLayer && Plugin.Layer != Layer) continue;
        if (bOnlyWithMaps && Plugin.DiscoveredMapCount == 0) continue;
        if (!Query.IsEmpty() &&
            !Plugin.PluginId.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
            !Plugin.DisplayName.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
            !Plugin.Description.Contains(Query, ESearchCase::IgnoreCase)) continue;
        Results.Add(Plugin);
    }
    return Results;
}

TArray<FDivineBeastsDemoScenario> UDivineBeastsDemoHubWidget::GetPluginScenarios(FName PluginId) const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetScenarios(PluginId) : TArray<FDivineBeastsDemoScenario>();
}

bool UDivineBeastsDemoHubWidget::RequestOpenScenario(FName ScenarioId)
{
    auto* Catalog = GetCatalog();
    return Catalog && Catalog->OpenScenario(ScenarioId);
}

bool UDivineBeastsDemoHubWidget::RequestReturnToHub()
{
    auto* Catalog = GetCatalog();
    return Catalog && Catalog->ReturnToHub();
}

FText UDivineBeastsDemoHubWidget::GetDemoOperationMessage() const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetLastOperationMessage()
        : FText::FromString(TEXT("演示目录未初始化或当前不是受控开发客户端。"));
}

// 下列事件只消费Monolith制作的真实WBP命名控件：C++不创建按钮、布局或视觉资源。
void UDivineBeastsDemoHubWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (PluginSearchInput) PluginSearchInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::OnSearchChanged);
    if (LayerFilterCombo)
    {
        // 分类中文标签同时适用于代码插件与项目内容包，不将内容包提升为第四逻辑层。
        for (const TCHAR* Name : {TEXT("全部层级"), TEXT("平台层"), TEXT("MOBA通用层"),
            TEXT("神兽联盟项目层"), TEXT("项目内容包")})
        {
            LayerFilterCombo->AddOption(Name);
        }
        LayerFilterCombo->SetSelectedIndex(0);
        LayerFilterCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnLayerChanged);
    }
    if (PluginSelectorCombo) PluginSelectorCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnPluginChanged);
    if (ScenarioSelectorCombo) ScenarioSelectorCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnScenarioChanged);
    if (RefreshButton) RefreshButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnCatalogRefreshClicked);
    if (ClearFilterButton) ClearFilterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnFilterResetClicked);
    if (LaunchScenarioButton) LaunchScenarioButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnScenarioLaunchClicked);
    if (ReturnButton) ReturnButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnHubReturnClicked);
    // BackButton只存在于覆盖层及鸟群专题；统一绑定一次，避免两次切图请求。
    // 鸟群派生类已拥有名为BackButton的反射属性；基类用名称定位，避免UHT属性重名遮蔽。
    if (auto* DemoBack = Cast<UButton>(GetWidgetFromName(TEXT("BackButton"))))
        DemoBack->OnClicked.AddUniqueDynamic(this, &ThisClass::OnHubReturnClicked);
    if (ResetDemoButton) ResetDemoButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnDemoResetClicked);
    if (PluginSelectorCombo) RefreshPluginControls();
    else UpdateOverlayStatus();
}

void UDivineBeastsDemoHubWidget::RefreshPluginControls()
{
    if (!PluginSelectorCombo) return;
    TGuardValue<bool> Updating(bSynchronizingDemoUi, true);
    const FString Previous = PluginSelectorCombo->GetSelectedOption();
    const FString Query = PluginSearchInput ? PluginSearchInput->GetText().ToString() : FString();
    const int32 LayerIndex = LayerFilterCombo ? LayerFilterCombo->GetSelectedIndex() : 0;
    const bool bLayerSelected = LayerIndex >= 1 && LayerIndex <= 4;
    const auto FilterLayer = bLayerSelected
        ? static_cast<EDivineBeastsDemoLayer>(LayerIndex - 1)
        : EDivineBeastsDemoLayer::Platform;
    const TArray<FDivineBeastsDemoPlugin> Results = FilterDiscoveredPlugins(
        Query, bLayerSelected, FilterLayer, false);
    PluginSelectorCombo->ClearOptions();
    for (const FDivineBeastsDemoPlugin& Plugin : Results)
    {
        PluginSelectorCombo->AddOption(Plugin.PluginId.ToString());
    }
    if (!Previous.IsEmpty() && PluginSelectorCombo->FindOptionIndex(Previous) != INDEX_NONE)
        PluginSelectorCombo->SetSelectedOption(Previous);
    else if (!Results.IsEmpty())
        PluginSelectorCombo->SetSelectedIndex(0);
    if (PluginCountText)
    {
        PluginCountText->SetText(FText::FromString(FString::Printf(
            TEXT("匹配插件：%d / 真实已发现插件：%d"), Results.Num(), GetDiscoveredPlugins().Num())));
    }
    RefreshScenarioControls();
}

void UDivineBeastsDemoHubWidget::RefreshScenarioControls()
{
    if (!ScenarioSelectorCombo) return;
    TGuardValue<bool> Updating(bSynchronizingDemoUi, true);
    const FString Previous = ScenarioSelectorCombo->GetSelectedOption();
    ScenarioSelectorCombo->ClearOptions();
    ScenarioByOption.Reset();
    const FName SelectedPlugin = PluginSelectorCombo
        ? FName(*PluginSelectorCombo->GetSelectedOption()) : NAME_None;
    FDivineBeastsDemoPlugin Selected;
    bool bPluginFound = false;
    for (const FDivineBeastsDemoPlugin& Plugin : GetDiscoveredPlugins())
    {
        if (Plugin.PluginId == SelectedPlugin)
        {
            Selected = Plugin;
            bPluginFound = true;
            break;
        }
    }
    if (PluginDescriptionText)
    {
        const FString Description = bPluginFound
            ? FString::Printf(TEXT("%s\n%s\n%s；已登记地图：%d"),
                *Selected.DisplayName.ToString(), *Selected.Description,
                Selected.bEnabled ? TEXT("插件已启用") : TEXT("插件未启用"),
                Selected.DiscoveredMapCount)
            : TEXT("当前筛选下没有可选择插件。");
        PluginDescriptionText->SetText(FText::FromString(Description));
    }
    if (bPluginFound && Selected.bEnabled)
    {
        for (const FDivineBeastsDemoScenario& Scenario : GetPluginScenarios(SelectedPlugin))
        {
            const FString Option = FString::Printf(TEXT("%s  |  %s"),
                *Scenario.Title.ToString(), *Scenario.ScenarioId.ToString());
            ScenarioByOption.Add(Option, Scenario.ScenarioId);
            ScenarioSelectorCombo->AddOption(Option);
        }
    }
    if (!Previous.IsEmpty() && ScenarioSelectorCombo->FindOptionIndex(Previous) != INDEX_NONE)
        ScenarioSelectorCombo->SetSelectedOption(Previous);
    else if (ScenarioSelectorCombo->GetOptionCount() > 0)
        ScenarioSelectorCombo->SetSelectedIndex(0);
    UpdateScenarioStatus();
}

void UDivineBeastsDemoHubWidget::UpdateScenarioStatus()
{
    const FString Selected = ScenarioSelectorCombo ? ScenarioSelectorCombo->GetSelectedOption() : FString();
    const FName* Id = ScenarioByOption.Find(Selected);
    bool bCanOpen = false;
    FText Status = FText::FromString(TEXT("该插件暂无真实可用场景；功能未完成不应冒充可演示。"));
    if (Id && PluginSelectorCombo)
    {
        for (const FDivineBeastsDemoScenario& Scenario :
            GetPluginScenarios(FName(*PluginSelectorCombo->GetSelectedOption())))
        {
            if (Scenario.ScenarioId == *Id)
            {
                Status = Scenario.VerificationStatus;
                bCanOpen = Scenario.bAvailable;
                break;
            }
        }
    }
    if (LaunchScenarioButton) LaunchScenarioButton->SetIsEnabled(bCanOpen);
    if (OperationStatusText) OperationStatusText->SetText(Status);
}

void UDivineBeastsDemoHubWidget::UpdateOverlayStatus()
{
    if (!OverlayFeedbackText) return;
    UWorld* World = GetWorld();
    const auto* Demo = World ? World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
    OverlayFeedbackText->SetText(Demo ? Demo->GetLastFeedback() :
        FText::FromString(TEXT("当前世界未获得本地演示状态。")));
}

void UDivineBeastsDemoHubWidget::OnSearchChanged(const FText& NewText)
{
    if (!bSynchronizingDemoUi) RefreshPluginControls();
}
void UDivineBeastsDemoHubWidget::OnLayerChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!bSynchronizingDemoUi) RefreshPluginControls();
}
void UDivineBeastsDemoHubWidget::OnPluginChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!bSynchronizingDemoUi) RefreshScenarioControls();
}
void UDivineBeastsDemoHubWidget::OnScenarioChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!bSynchronizingDemoUi) UpdateScenarioStatus();
}
void UDivineBeastsDemoHubWidget::OnCatalogRefreshClicked()
{
    if (auto* Catalog = GetCatalog()) Catalog->RefreshCatalog();
    RefreshPluginControls();
}
void UDivineBeastsDemoHubWidget::OnFilterResetClicked()
{
    TGuardValue<bool> Updating(bSynchronizingDemoUi, true);
    if (PluginSearchInput) PluginSearchInput->SetText(FText::GetEmpty());
    if (LayerFilterCombo) LayerFilterCombo->SetSelectedIndex(0);
    RefreshPluginControls();
}
void UDivineBeastsDemoHubWidget::OnScenarioLaunchClicked()
{
    const FString Selected = ScenarioSelectorCombo ? ScenarioSelectorCombo->GetSelectedOption() : FString();
    const FName* Id = ScenarioByOption.Find(Selected);
    if (Id) RequestOpenScenario(*Id);
    if (OperationStatusText) OperationStatusText->SetText(GetDemoOperationMessage());
}
void UDivineBeastsDemoHubWidget::OnHubReturnClicked()
{
    const bool bAccepted = RequestReturnToHub();
    if (OperationStatusText) OperationStatusText->SetText(GetDemoOperationMessage());
    // 材质审核页只有OverlayFeedbackText；拒绝返回时必须显示目录错误，不能被旧世界反馈覆盖。
    // true只代表目录受理，是否已完成旅行仍由实际世界事件说明。
    if (!bAccepted && OverlayFeedbackText) OverlayFeedbackText->SetText(GetDemoOperationMessage());
    else UpdateOverlayStatus();
}
void UDivineBeastsDemoHubWidget::OnDemoResetClicked()
{
    UWorld* World = GetWorld();
    if (auto* Demo = World ? World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr)
        Demo->ResetDemoChanges();
    UpdateOverlayStatus();
}

