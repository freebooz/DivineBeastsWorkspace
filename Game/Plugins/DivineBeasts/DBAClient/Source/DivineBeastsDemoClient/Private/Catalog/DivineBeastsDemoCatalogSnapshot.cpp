// SSLM目录查询/原子发布实现；仅游戏线程，定义检查无加载副作用，复制值不延长资产租约。
#include "Catalog/DivineBeastsDemoCatalogSnapshot.h"
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"

namespace DivineBeasts::Demo
{
FGamePlatformResult FCatalogSnapshot::Publish(const UDivineBeastsDemoCatalogDefinition& Definition,
    const TArray<FDivineBeastsDemoPlugin>& DiscoveredPlugins)
{
    const FGamePlatformResult Valid = Definition.ValidateDefinition();
    if (!Valid.IsSuccess()) return Valid;
    TSet<FName> Actual;
    for (const auto& Plugin : DiscoveredPlugins) Actual.Add(Plugin.PluginId);
    if (Actual.Num() != DiscoveredPlugins.Num() || Actual.Num() != Definition.Plugins.Num())
        return FGamePlatformResult::Failure(TEXT("Demo.PluginSetMismatch"), TEXT("目录与实际发现插件集合数量不一致，保留旧目录。"));
    for (const auto& Plugin : Definition.Plugins)
        if (!Actual.Contains(Plugin.PluginId))
            return FGamePlatformResult::Failure(TEXT("Demo.PluginSetMismatch"), TEXT("目录含未发现插件，保留旧目录。"));
    // 一切拒绝在写入前完成；不向订阅者发布部分功能或半份检查合同。
    Features = Definition.Features;
    Scenarios = Definition.Scenarios;
    Reviews = Definition.ReviewCases;
    LegacyRoutes = Definition.LegacyMapRoutes;
    SourceFingerprint = Definition.SourceFingerprint;
    bPublished = true;
    return FGamePlatformResult::Success();
}
void FCatalogSnapshot::Reset()
{
    Features.Reset(); Scenarios.Reset(); Reviews.Reset(); LegacyRoutes.Reset(); SourceFingerprint.Reset(); bPublished = false;
}
TArray<FDivineBeastsDemoFeatureEntry> FCatalogSnapshot::GetFeatures(FName PluginId) const
{
    TArray<FDivineBeastsDemoFeatureEntry> Result;
    for (const auto& Feature : Features) if (Feature.PluginId == PluginId) Result.Add(Feature);
    return Result;
}
TArray<FDivineBeastsDemoScenarioDefinition> FCatalogSnapshot::GetFeatureScenarios(FName FeatureId) const
{
    TArray<FDivineBeastsDemoScenarioDefinition> Result;
    for (const auto& Scenario : Scenarios)
        if (Scenario.FeatureId == FeatureId || Scenario.RelatedFeatureIds.Contains(FeatureId)) Result.Add(Scenario);
    return Result;
}
const FDivineBeastsDemoScenarioDefinition* FCatalogSnapshot::FindScenario(FName ScenarioId) const
{
    return Scenarios.FindByPredicate([ScenarioId](const auto& Scenario) { return Scenario.ScenarioId == ScenarioId; });
}
FGamePlatformResult FCatalogSnapshot::ResolveIdentity(FName InputId, FName& OutId) const
{
    OutId = NAME_None;
    if (FindScenario(InputId)) { OutId = InputId; return FGamePlatformResult::Success(); }
    for (const auto& Route : LegacyRoutes)
    {
        if (FName(*Route.MapAsset.GetLongPackageName()) != InputId) continue;
        if (Route.bRequiresSelection || Route.ScenarioIds.Num() != 1)
            return FGamePlatformResult::Failure(TEXT("Demo.ScenarioSelectionRequired"), TEXT("旧地图对应多个案例，请按独立案例身份选择。"));
        OutId = Route.ScenarioIds[0];
        return FGamePlatformResult::Success();
    }
    return FGamePlatformResult::Failure(TEXT("Demo.ScenarioNotRegistered"), TEXT("案例身份及旧地图路由均未登记。"));
}
}
