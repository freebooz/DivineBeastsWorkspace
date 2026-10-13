// SSLM开发菜单事件投影：作用域服务拥有事实，页面只筛选值和转发受控命令，不扫描资产。
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/World.h"

namespace
{
/** 三层筛选保留旧Content显示枚举，但内容包的架构归属始终为Project。 */
FName PluginLayer(const FDivineBeastsDemoPlugin& Plugin)
{
    if (Plugin.Layer == EDivineBeastsDemoLayer::Platform) return TEXT("Platform");
    if (Plugin.Layer == EDivineBeastsDemoLayer::Moba) return TEXT("Moba");
    return TEXT("Project");
}
/** 搜索只比较中文名称/稳定身份/目的，不解析或执行作者文本。 */
bool FeatureMatchesText(const FDivineBeastsDemoFeatureEntry& Feature, const FString& Query)
{
    return Query.IsEmpty() || Feature.FeatureId.ToString().Contains(Query) || Feature.Title.ToString().Contains(Query) || Feature.Purpose.Contains(Query);
}
}

bool UDivineBeastsDemoHubViewModel::BindCatalog(UDivineBeastsDemoCatalogSubsystem* InCatalog)
{
    check(IsInGameThread());
    DetachSubscriptions(); Catalog = InCatalog;
    if (IsPageActive()) Subscribe();
    RebuildSnapshot(); return InCatalog != nullptr;
}
void UDivineBeastsDemoHubViewModel::DetachSubscriptions()
{
    if (auto* Source = Catalog.Get()) Source->OnCatalogChanged.Remove(CatalogHandle);
    if (auto* World = BoundWorld.Get()) World->OnRunChanged.Remove(RunHandle);
    CatalogHandle.Reset(); RunHandle.Reset(); BoundWorld.Reset();
}
void UDivineBeastsDemoHubViewModel::UnbindCatalog() { DetachSubscriptions(); Catalog.Reset(); Snapshot = {}; }
void UDivineBeastsDemoHubViewModel::Subscribe()
{
    if (!IsPageActive() || !Catalog.IsValid()) return;
    if (!CatalogHandle.IsValid()) CatalogHandle = Catalog->OnCatalogChanged.AddUObject(this, &ThisClass::RebuildSnapshot);
    auto* World = Catalog->GetWorld() ? Catalog->GetWorld()->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
    if (World == BoundWorld.Get()) return;
    if (auto* Previous = BoundWorld.Get()) Previous->OnRunChanged.Remove(RunHandle);
    BoundWorld = World; RunHandle.Reset();
    if (World) RunHandle = World->OnRunChanged.AddUObject(this, &ThisClass::OnRunSnapshotChanged);
}
void UDivineBeastsDemoHubViewModel::OnPageBegan() { Subscribe(); RebuildSnapshot(); }
void UDivineBeastsDemoHubViewModel::OnPageEnded() { DetachSubscriptions(); }
void UDivineBeastsDemoHubViewModel::BeginDestroy() { UnbindCatalog(); Super::BeginDestroy(); }
void UDivineBeastsDemoHubViewModel::OnRunSnapshotChanged(const FDivineBeastsDemoRunSnapshot&)
{ if (IsPageActive()) RebuildSnapshot(); }
void UDivineBeastsDemoHubViewModel::SetFilter(const FDivineBeastsDemoFilter& InFilter)
{ Filter = InFilter; RebuildSnapshot(); }
bool UDivineBeastsDemoHubViewModel::SelectPlugin(FName Id)
{
    if (!Snapshot.Plugins.ContainsByPredicate([Id](const auto& Item) { return Item.PluginId == Id; })) return false;
    Snapshot.SelectedPluginId = Id; Snapshot.SelectedFeatureId = NAME_None; Snapshot.SelectedScenarioId = NAME_None;
    RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::SelectFeature(FName Id)
{
    if (!Snapshot.Features.ContainsByPredicate([Id](const auto& Item) { return Item.Feature.FeatureId == Id; })) return false;
    Snapshot.SelectedFeatureId = Id; Snapshot.SelectedScenarioId = NAME_None; RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::SelectScenario(FName Id)
{
    if (!Snapshot.Scenarios.ContainsByPredicate([Id](const auto& Item) { return Item.Scenario.ScenarioId == Id; })) return false;
    Snapshot.SelectedScenarioId = Id; RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::RequestScenario(FName Id)
{
    if (!IsPageActive() || !Catalog.IsValid() || !Catalog->PreflightScenario(Id).bCanStart) return false;
    return Catalog->OpenScenario(Id);
}
bool UDivineBeastsDemoHubViewModel::RequestOperation(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values)
{ return IsPageActive() && BoundWorld.IsValid() && BoundWorld->RequestScenarioOperation(Operation, Values); }
void UDivineBeastsDemoHubViewModel::RequestRefresh() { if (IsPageActive() && Catalog.IsValid()) Catalog->RefreshCatalog(); }
bool UDivineBeastsDemoHubViewModel::RequestReturn() { return IsPageActive() && Catalog.IsValid() && Catalog->ReturnToHub(); }
UUserWidget* UDivineBeastsDemoHubViewModel::GetSampleWidget() const { return BoundWorld.IsValid() ? BoundWorld->GetSampleWidget() : nullptr; }
FString UDivineBeastsDemoHubViewModel::RequestEvidence(FString& OutError)
{
    FString Path;
    if (!IsPageActive() || !BoundWorld.IsValid()) { OutError = TEXT("页面失活或无实际世界运行。"); return Path; }
    BoundWorld->WriteCurrentEvidence(Path, OutError); return Path;
}

void UDivineBeastsDemoHubViewModel::RebuildSnapshot()
{
    check(IsInGameThread());
    if (IsPageActive()) Subscribe();
    const FName PreviousPlugin = Snapshot.SelectedPluginId, PreviousFeature = Snapshot.SelectedFeatureId, PreviousScenario = Snapshot.SelectedScenarioId;
    Snapshot = {};
    if (!Catalog.IsValid()) { Snapshot.Details = FText::FromString(TEXT("目录服务不可用。")); MarkStateChanged(); return; }
    Snapshot.Run = BoundWorld.IsValid() ? BoundWorld->GetRunSnapshot() : FDivineBeastsDemoRunSnapshot();
    Snapshot.CatalogResult = Catalog->GetCatalogResult(); Snapshot.SourceFingerprint = Catalog->GetSourceFingerprint();
    const auto AllPlugins = Catalog->GetPlugins(); Snapshot.TotalPlugins = AllPlugins.Num();
    const FString Query = Filter.SearchText.TrimStartAndEnd();
    for (const auto& Plugin : AllPlugins)
    {
        if (Plugin.FeatureAuditStatus != TEXT("complete")) ++Snapshot.UnreviewedPlugins;
        const auto Features = Catalog->GetFeatures(Plugin.PluginId); Snapshot.KnownFeatures += Features.Num();
        bool bFeatureMatches = false;
        for (const auto& Feature : Features)
        {
            if (Feature.IntegrationStatus == TEXT("wired") || Feature.IntegrationStatus == TEXT("launchable")) ++Snapshot.WiredFeatures;
            bFeatureMatches |= FeatureMatchesText(Feature, Query);
        }
        const FName Kind = Plugin.Layer == EDivineBeastsDemoLayer::Content ? FName(TEXT("content_pack")) : FName(TEXT("code"));
        if ((!Filter.Layer.IsNone() && Filter.Layer != PluginLayer(Plugin)) || (!Filter.PluginKind.IsNone() && Filter.PluginKind != Kind)) continue;
        if (!Query.IsEmpty() && !Plugin.PluginId.ToString().Contains(Query) && !Plugin.DisplayName.ToString().Contains(Query) &&
            !Plugin.Description.Contains(Query) && !bFeatureMatches) continue;
        Snapshot.Plugins.Add(Plugin);
    }
    Snapshot.bHasKnownDenominator = Snapshot.UnreviewedPlugins == 0 && Snapshot.KnownFeatures > 0;
    Snapshot.SelectedPluginId = Snapshot.Plugins.ContainsByPredicate([PreviousPlugin](const auto& P) { return P.PluginId == PreviousPlugin; })
        ? PreviousPlugin : (Snapshot.Plugins.IsEmpty() ? NAME_None : Snapshot.Plugins[0].PluginId);
    for (const auto& Feature : Catalog->GetFeatures(Snapshot.SelectedPluginId))
    {
        if (!FeatureMatchesText(Feature, Query))
        {
            const auto* Plugin = Snapshot.Plugins.FindByPredicate([this](const auto& P) { return P.PluginId == Snapshot.SelectedPluginId; });
            if (!Plugin || (!Plugin->PluginId.ToString().Contains(Query) && !Plugin->DisplayName.ToString().Contains(Query) && !Plugin->Description.Contains(Query))) continue;
        }
        FDivineBeastsDemoFeatureMenuEntry Row; Row.Feature = Feature;
        if (Snapshot.Run.FeatureId == Feature.FeatureId) Row.RunState = Snapshot.Run.State;
        if ((!Filter.ImplementationStatus.IsNone() && Filter.ImplementationStatus != Feature.ImplementationStatus) ||
            (!Filter.IntegrationStatus.IsNone() && Filter.IntegrationStatus != Feature.IntegrationStatus) ||
            (Filter.bFilterRunState && Row.RunState != Filter.RunState) ||
            (!Filter.ReviewStatus.IsNone() && Filter.ReviewStatus != Row.ReviewStatus)) continue;
        bool bModeMatches = Filter.RunMode.IsNone();
        for (const auto& Scenario : Catalog->GetFeatureScenarios(Feature.FeatureId)) bModeMatches |= Scenario.RunMode == Filter.RunMode;
        if (!bModeMatches) continue;
        Snapshot.Features.Add(MoveTemp(Row));
    }
    Snapshot.SelectedFeatureId = Snapshot.Features.ContainsByPredicate([PreviousFeature](const auto& F) { return F.Feature.FeatureId == PreviousFeature; })
        ? PreviousFeature : (Snapshot.Features.IsEmpty() ? NAME_None : Snapshot.Features[0].Feature.FeatureId);
    for (const auto& Scenario : Catalog->GetFeatureScenarios(Snapshot.SelectedFeatureId))
    {
        if (!Filter.RunMode.IsNone() && Filter.RunMode != Scenario.RunMode) continue;
        FDivineBeastsDemoScenarioMenuEntry Row; Row.Scenario = Scenario; Row.Preflight = Catalog->PreflightScenario(Scenario.ScenarioId);
        Snapshot.Scenarios.Add(MoveTemp(Row));
    }
    Snapshot.SelectedScenarioId = Snapshot.Scenarios.ContainsByPredicate([PreviousScenario](const auto& S) { return S.Scenario.ScenarioId == PreviousScenario; })
        ? PreviousScenario : (Snapshot.Scenarios.IsEmpty() ? NAME_None : Snapshot.Scenarios[0].Scenario.ScenarioId);
    FString Details;
    for (const auto& Row : Snapshot.Features)
        if (Row.Feature.FeatureId == Snapshot.SelectedFeatureId)
        {
            Details += Row.Feature.Title.ToString() + TEXT("\n") + Row.Feature.Purpose + TEXT("\n实现：") + Row.Feature.ImplementationStatus.ToString() +
                TEXT("；接入：") + Row.Feature.IntegrationStatus.ToString() + TEXT("；人工：尚未核验\n");
            for (const auto& Evidence : Row.Feature.ImplementationEvidence) Details += Evidence.Path + TEXT(" :: ") + Evidence.Symbol + TEXT("\n");
        }
    for (const auto& Row : Snapshot.Scenarios)
        if (Row.Scenario.ScenarioId == Snapshot.SelectedScenarioId)
        {
            Details += TEXT("运行方式：") + Row.Scenario.RunMode.ToString() + TEXT("\n");
            for (const auto& Precondition : Row.Scenario.Preconditions) Details += TEXT("前提：") + Precondition + TEXT("\n");
            for (const auto& Reason : Row.Preflight.BlockingReasons) Details += TEXT("阻塞：") + Reason.ToString() + TEXT("\n");
            for (const auto& Review : Catalog->GetReviewCases()) if (Row.Scenario.ReviewCaseIds.Contains(Review.ReviewCaseId))
            {
                Details += TEXT("检查：") + Review.ReviewCaseId.ToString() + TEXT("\n");
                for (const auto& Step : Review.Steps) Details += TEXT("步骤：") + Step + TEXT("\n");
                Details += TEXT("预期：") + Review.ExpectedResult + TEXT("\n");
            }
        }
    if (Details.IsEmpty()) Details = TEXT("功能范围待审计或筛选无结果；请查看目录诊断，不能据此认为插件没有功能。\n") + Snapshot.CatalogResult.Message;
    Snapshot.Details = FText::FromString(Details); MarkStateChanged();
}
