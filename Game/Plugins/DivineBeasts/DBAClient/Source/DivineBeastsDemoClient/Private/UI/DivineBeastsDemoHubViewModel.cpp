// SSLM开发菜单事件投影：作用域服务拥有事实，页面只筛选值和转发受控命令，不扫描资产。
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/World.h"
#include "Development/DivineBeastsPluginDemoMode.h"

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
    return Query.IsEmpty() || Feature.FeatureId.ToString().Contains(Query, ESearchCase::IgnoreCase) ||
        Feature.Title.ToString().Contains(Query, ESearchCase::IgnoreCase) ||
        Feature.Purpose.Contains(Query, ESearchCase::IgnoreCase);
}
#if WITH_EDITOR
/** 已审查开发地图与功能领域的显式诊断关联；仅便利人工查找，不改变资产所有权、功能案例及签收。 */
bool IsKnownDiagnosticMapForPlugin(FName PluginId, const FString& Package)
{
    if (PluginId == TEXT("GamePlatformPCG"))
        return Package == TEXT("/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1");
    if (PluginId == TEXT("GamePlatformWater"))
        return Package == TEXT("/Game/Development/Water/Maps/L_GP_WaterReview");
    if (PluginId == TEXT("GamePlatformWeather"))
        return Package == TEXT("/Game/Development/Weather/Maps/L_DBA_WeatherReview");
    if (PluginId == TEXT("GamePlatformCamera"))
        return Package == TEXT("/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview");
    if (PluginId == TEXT("GamePlatformCombat"))
        return Package == TEXT("/Game/Development/Foundation/PluginReviews/Maps/L_GP_CombatReview");
    if (PluginId == TEXT("GamePlatformSFX"))
        return Package == TEXT("/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview");
    if (PluginId == TEXT("GamePlatformInteraction") || PluginId == TEXT("GamePlatformAI"))
        return Package == TEXT("/Game/Development/Foundation/PluginReviews/Maps/L_GP_InteractionAIReview");
    if (PluginId == TEXT("GamePlatformSurface"))
        return Package == TEXT("/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview");
    if (PluginId == TEXT("GamePlatformWorld") || PluginId == TEXT("DBAWorlds"))
        return Package == TEXT("/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview");
    if (PluginId == TEXT("GamePlatformAnimation") || PluginId == TEXT("GamePlatformCharacter") ||
        PluginId == TEXT("DBAGameplay"))
        return Package == TEXT("/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase");
    if (PluginId == TEXT("GamePlatformArena") || PluginId == TEXT("DBAArena"))
        return Package == TEXT("/Game/Development/Foundation/PluginReviews/Maps/L_DBA_MainArenaReview");
    if (PluginId == TEXT("DBAClient"))
        return Package == TEXT("/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview");
    if (PluginId == TEXT("GamePlatformVFX") || PluginId == TEXT("DBAPresentationPack_Core"))
        return Package == TEXT("/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/L_DBA_IceArrow_A_Review");
    return false;
}
#endif
}

bool UDivineBeastsDemoHubViewModel::BindCatalog(UDivineBeastsDemoCatalogSubsystem* InCatalog)
{
    check(IsInGameThread());
    DetachSubscriptions(); Catalog = InCatalog;
    if (IsPageActive()) Subscribe();
    RebuildSnapshot(); return InCatalog != nullptr;
}
bool UDivineBeastsDemoHubViewModel::ConfigurePluginContext(FName Id)
{
    if (!Id.IsNone() && !FDivineBeastsPluginDemoMode::IsValidPluginIdText(Id.ToString())) return false;
    if (Id != FixedPluginId && IsPageActive() && Catalog.IsValid()) return false;
    if (Id != FixedPluginId) { FixedPluginId = Id; Filter = {}; Snapshot = {}; }
    return true;
}
bool UDivineBeastsDemoHubViewModel::IsPluginEnabled(FName Id) const
{
    return Catalog.IsValid() && Catalog->GetPlugins().ContainsByPredicate([Id](const auto& Plugin)
        { return Plugin.PluginId == Id && Plugin.bEnabled; });
}
bool UDivineBeastsDemoHubViewModel::OwnsFeature(FName Id) const
{
    return FixedPluginId.IsNone() || (Catalog.IsValid() && Catalog->GetFeatures(FixedPluginId).ContainsByPredicate(
        [Id](const auto& Feature) { return Feature.FeatureId == Id; }));
}
bool UDivineBeastsDemoHubViewModel::RequestPluginPage(FName Id)
{
    return IsPageActive() && Catalog.IsValid() && (FixedPluginId.IsNone() || FixedPluginId == Id) &&
        Snapshot.Plugins.ContainsByPredicate([Id](const auto& Plugin) { return Plugin.PluginId == Id; }) && Catalog->OpenPluginPage(Id);
}
void UDivineBeastsDemoHubViewModel::DetachSubscriptions()
{
    ++BindingGeneration;
    if (auto* Source = Catalog.Get()) Source->OnCatalogChanged.Remove(CatalogHandle);
    if (auto* World = BoundWorld.Get()) World->OnRunChanged.Remove(RunHandle);
    CatalogHandle.Reset(); RunHandle.Reset(); BoundWorld.Reset();
}
void UDivineBeastsDemoHubViewModel::UnbindCatalog() { DetachSubscriptions(); Catalog.Reset(); Snapshot = {}; Filter = {}; }
void UDivineBeastsDemoHubViewModel::Subscribe()
{
    if (!IsPageActive() || !Catalog.IsValid()) return;
    const uint64 Generation = BindingGeneration;
    const int32 Page = GetPageGeneration();
    const TWeakObjectPtr<UDivineBeastsDemoCatalogSubsystem> Source = Catalog;
    if (!CatalogHandle.IsValid()) CatalogHandle = Catalog->OnCatalogChanged.AddWeakLambda(this, [this, Source, Generation, Page]()
    { if (IsPageActive() && Catalog == Source && BindingGeneration == Generation && GetPageGeneration() == Page) RebuildSnapshot(); });
    auto* World = Catalog->GetWorld() ? Catalog->GetWorld()->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
    if (World == BoundWorld.Get()) return;
    if (auto* Previous = BoundWorld.Get()) Previous->OnRunChanged.Remove(RunHandle);
    BoundWorld = World; RunHandle.Reset();
    const TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WorldSource = World;
    if (World) RunHandle = World->OnRunChanged.AddWeakLambda(this, [this, WorldSource, Generation, Page](const auto& Run)
    { if (IsPageActive() && BoundWorld == WorldSource && BindingGeneration == Generation && GetPageGeneration() == Page) OnRunSnapshotChanged(Run); });
}
void UDivineBeastsDemoHubViewModel::OnPageBegan() { DetachSubscriptions(); Subscribe(); RebuildSnapshot(); }
void UDivineBeastsDemoHubViewModel::OnPageEnded() { DetachSubscriptions(); Snapshot = {}; Filter = {}; }
void UDivineBeastsDemoHubViewModel::BeginDestroy() { UnbindCatalog(); Super::BeginDestroy(); }
void UDivineBeastsDemoHubViewModel::OnRunSnapshotChanged(const FDivineBeastsDemoRunSnapshot&)
{ if (IsPageActive()) RebuildSnapshot(); }
void UDivineBeastsDemoHubViewModel::SetFilter(const FDivineBeastsDemoFilter& InFilter)
{ if (IsPageActive()) { Filter = InFilter; RebuildSnapshot(); } }
bool UDivineBeastsDemoHubViewModel::SelectPlugin(FName Id)
{
    if (!IsPageActive() || (!FixedPluginId.IsNone() && Id != FixedPluginId)) return false;
    if (!Snapshot.Plugins.ContainsByPredicate([Id](const auto& Item) { return Item.PluginId == Id; })) return false;
    Snapshot.SelectedPluginId = Id; Snapshot.SelectedFeatureId = NAME_None; Snapshot.SelectedScenarioId = NAME_None;
    RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::SelectFeature(FName Id)
{
    if (!IsPageActive() || (!FixedPluginId.IsNone() && !Snapshot.Features.ContainsByPredicate([this, Id](const auto& Row)
        { return Row.Feature.FeatureId == Id && Row.Feature.PluginId == FixedPluginId; }))) return false;
    if (!Snapshot.Features.ContainsByPredicate([Id](const auto& Item) { return Item.Feature.FeatureId == Id; })) return false;
    Snapshot.SelectedFeatureId = Id; Snapshot.SelectedScenarioId = NAME_None; RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::SelectScenario(FName Id)
{
    if (!IsPageActive()) return false;
    if (!FixedPluginId.IsNone() && !Snapshot.Scenarios.ContainsByPredicate([this, Id](const auto& Row)
        { return Row.Scenario.ScenarioId == Id && OwnsFeature(Row.Scenario.FeatureId); })) return false;
    if (!Snapshot.Scenarios.ContainsByPredicate([Id](const auto& Item) { return Item.Scenario.ScenarioId == Id; })) return false;
    Snapshot.SelectedScenarioId = Id; RebuildSnapshot(); return true;
}
bool UDivineBeastsDemoHubViewModel::RequestScenario(FName Id)
{
    if (!IsPageActive() || !Catalog.IsValid()) return false;
    // 原生开发地图可由真实AssetRegistry发现，但属于诊断资料，不是正式功能案例。
    // 只允许当前列表中通过只读预检的项进入，实际OpenScenario仍再次验证开发模式和地图包。
    const auto* Selected = Snapshot.Scenarios.FindByPredicate([Id](const auto& Item)
    {
        return Item.Scenario.ScenarioId == Id;
    });
    if (!Selected || !Selected->Preflight.bCanStart || !IsPluginEnabled(Snapshot.SelectedPluginId) ||
        (!FixedPluginId.IsNone() && Snapshot.SelectedPluginId != FixedPluginId)) return false;
    if (Selected->Scenario.TemplateId == TEXT("DiagnosticMap"))
    {
        if (!FixedPluginId.IsNone() && FixedPluginId != TEXT("GamePlatformDeveloperTools")) return false;
        return Catalog->OpenScenario(Id);
    }
    return OwnsFeature(Selected->Scenario.FeatureId) && Catalog->PreflightScenario(Id).bCanStart && Catalog->OpenScenario(Id);
}
bool UDivineBeastsDemoHubViewModel::RequestOperation(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values)
{
    return IsPageActive() && BoundWorld.IsValid() && IsPluginEnabled(Snapshot.SelectedPluginId) &&
        (FixedPluginId.IsNone() || Snapshot.SelectedPluginId == FixedPluginId) &&
        OwnsFeature(BoundWorld->GetRunSnapshot().FeatureId) && BoundWorld->RequestScenarioOperation(Operation, Values);
}
void UDivineBeastsDemoHubViewModel::RequestRefresh() { if (IsPageActive() && Catalog.IsValid()) Catalog->RefreshCatalog(); }
bool UDivineBeastsDemoHubViewModel::RequestReturn() { return IsPageActive() && Catalog.IsValid() && Catalog->ReturnToHub(); }
UUserWidget* UDivineBeastsDemoHubViewModel::GetSampleWidget() const
{ return IsPageActive() && BoundWorld.IsValid() && OwnsFeature(BoundWorld->GetRunSnapshot().FeatureId) ? BoundWorld->GetSampleWidget() : nullptr; }
FString UDivineBeastsDemoHubViewModel::RequestEvidence(FString& OutError)
{
    FString Path;
    if (!IsPageActive() || !BoundWorld.IsValid() || !OwnsFeature(BoundWorld->GetRunSnapshot().FeatureId))
    { OutError = TEXT("页面失活、无实际世界运行或运行不属于当前固定插件。"); return Path; }
    BoundWorld->WriteCurrentEvidence(Path, OutError); return Path;
}

void UDivineBeastsDemoHubViewModel::RebuildSnapshot()
{
    check(IsInGameThread());
    if (IsPageActive()) Subscribe();
    const FName PreviousPlugin = Snapshot.SelectedPluginId, PreviousFeature = Snapshot.SelectedFeatureId, PreviousScenario = Snapshot.SelectedScenarioId;
    Snapshot = {};
    Snapshot.FixedPluginId = FixedPluginId;
    if (!Catalog.IsValid()) { Snapshot.Details = FText::FromString(TEXT("目录服务不可用。")); MarkStateChanged(); return; }
    Snapshot.Run = BoundWorld.IsValid() ? BoundWorld->GetRunSnapshot() : FDivineBeastsDemoRunSnapshot();
    if (!OwnsFeature(Snapshot.Run.FeatureId)) Snapshot.Run = {};
    Snapshot.CatalogResult = Catalog->GetCatalogResult(); Snapshot.SourceFingerprint = Catalog->GetSourceFingerprint();
    Snapshot.PageNavigationResult = Catalog->GetPageNavigationResult();
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
        if (!FixedPluginId.IsNone())
        {
            if (Plugin.PluginId == FixedPluginId) Snapshot.Plugins.Add(Plugin);
            continue;
        }
        const FName Kind = Plugin.Layer == EDivineBeastsDemoLayer::Content ? FName(TEXT("content_pack")) : FName(TEXT("code"));
        if ((!Filter.Layer.IsNone() && Filter.Layer != PluginLayer(Plugin)) || (!Filter.PluginKind.IsNone() && Filter.PluginKind != Kind)) continue;
        if (!Query.IsEmpty() &&
            !Plugin.PluginId.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
            !Plugin.DisplayName.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
            !Plugin.Description.Contains(Query, ESearchCase::IgnoreCase) && !bFeatureMatches) continue;
        Snapshot.Plugins.Add(Plugin);
    }
    Snapshot.bHasKnownDenominator = Snapshot.UnreviewedPlugins == 0 && Snapshot.KnownFeatures > 0;
    Snapshot.SelectedPluginId = Snapshot.Plugins.ContainsByPredicate([PreviousPlugin](const auto& P) { return P.PluginId == PreviousPlugin; })
        ? PreviousPlugin : (Snapshot.Plugins.IsEmpty() ? NAME_None : Snapshot.Plugins[0].PluginId);
    FSoftObjectPath PluginPage;
    Snapshot.bCanOpenPluginPage = Catalog->FindPluginPageAsset(Snapshot.SelectedPluginId, PluginPage);
    for (const auto& Feature : Catalog->GetFeatures(Snapshot.SelectedPluginId))
    {
        if (!FeatureMatchesText(Feature, Query))
        {
            const auto* Plugin = Snapshot.Plugins.FindByPredicate([this](const auto& P) { return P.PluginId == Snapshot.SelectedPluginId; });
            if (!Plugin || (!Plugin->PluginId.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
                !Plugin->DisplayName.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
                !Plugin->Description.Contains(Query, ESearchCase::IgnoreCase))) continue;
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
        if (!OwnsFeature(Scenario.FeatureId)) continue;
        if (!Filter.RunMode.IsNone() && Filter.RunMode != Scenario.RunMode) continue;
        FDivineBeastsDemoScenarioMenuEntry Row; Row.Scenario = Scenario; Row.Preflight = Catalog->PreflightScenario(Scenario.ScenarioId);
        Snapshot.Scenarios.Add(MoveTemp(Row));
    }
#if WITH_EDITOR
    // 已有UE5.8真实开发地图可在开发工具列表，或明确关联的领域插件下直接选择。
    // 映射仅是人工观察快捷入口，不向正式功能清单、操作适配器或人工通过数写入任何记录。
    const FName DiagnosticPluginId = Snapshot.SelectedPluginId;
    const FDivineBeastsDemoPlugin* SelectedDiagnosticPlugin = Snapshot.Plugins.FindByPredicate(
        [DiagnosticPluginId](const FDivineBeastsDemoPlugin& Item)
        {
            return Item.PluginId == DiagnosticPluginId;
        });
    if (SelectedDiagnosticPlugin && Snapshot.SelectedFeatureId.IsNone() &&
        (FixedPluginId.IsNone() || FixedPluginId == TEXT("GamePlatformDeveloperTools")) &&
        (SelectedDiagnosticPlugin->bEnabled ||
            SelectedDiagnosticPlugin->PluginId == TEXT("GamePlatformDeveloperTools")) &&
        (Filter.RunMode.IsNone() || Filter.RunMode == TEXT("editor")))
    {
        // 诊断地图只有明确所属插件启用时才开放；开发工具分组例外：它仅用于已有地图导航。
        for (const FDivineBeastsDemoScenario& Candidate :
            Catalog->GetScenarios(TEXT("GamePlatformDeveloperTools")))
        {
            if (!Candidate.bDiagnosticOnly || Candidate.MapAsset.IsNull()) continue;
            const FString Package = Candidate.MapAsset.GetLongPackageName();
            if (Snapshot.SelectedPluginId != TEXT("GamePlatformDeveloperTools") &&
                !IsKnownDiagnosticMapForPlugin(Snapshot.SelectedPluginId, Package)) continue;
            FDivineBeastsDemoScenarioMenuEntry Row;
            Row.Scenario.ScenarioId = Candidate.ScenarioId;
            Row.Scenario.Title = FText::FromString(
                FString(TEXT("开发诊断地图：")) + Candidate.Title.ToString());
            Row.Scenario.TemplateId = TEXT("DiagnosticMap");
            Row.Scenario.RunMode = TEXT("editor");
            Row.Scenario.MapAsset = Candidate.MapAsset;
            Row.Scenario.Preconditions.Add(
                TEXT("仅进入已存在的开发地图作人工观察；不是已接线功能或正式人工签收。"));
            // AssetRegistry目录已在Catalog刷新时判断资产存在；搜索/切换行不再逐项
            // 同步访问磁盘。真正启动仍由Catalog.OpenScenario验证PackageName并可安全拒绝。
            Row.Preflight.bCanStart = Candidate.bAvailable;
            Row.Preflight.Result = Row.Preflight.bCanStart
                ? FGamePlatformResult::Success()
                : FGamePlatformResult::Failure(TEXT("Demo.DiagnosticMapUnavailable"),
                    TEXT("开发诊断地图不可用；不可创建假入口。"));
            Snapshot.Scenarios.Add(MoveTemp(Row));
        }
    }
#endif
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
            if (Row.Scenario.TemplateId == TEXT("DiagnosticMap"))
            {
                Details += TEXT("此场景是实际存在的UE开发地图；只能用于人工观察、性能采样和回退验证。\n");
                Details += TEXT("尚未接入正式插件功能、没有独立操作适配器，也不提供自动人工通过。\n");
                Details += TEXT("入口操作：选中地图→启动→用已有场景界面人工观察→返回演示总览。\n");
            }

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
