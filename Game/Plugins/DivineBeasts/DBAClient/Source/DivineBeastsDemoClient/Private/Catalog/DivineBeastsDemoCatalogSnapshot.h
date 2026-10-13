// SSLM项目开发目录完整快照；仅模块内部使用，拥有值副本，不拥有UObject或资产管理职责。
#pragma once
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
class UDivineBeastsDemoCatalogDefinition;

namespace DivineBeasts::Demo
{
/** 游戏线程原子发布目录；失败保持旧完整快照，查询和筛选不改变底层分母。 */
class FCatalogSnapshot
{
public:
    /** 校验定义与真实发现插件集合后一次复制；禁用插件保留，失败返回具名原因。 */
    FGamePlatformResult Publish(const UDivineBeastsDemoCatalogDefinition& Definition,
        const TArray<FDivineBeastsDemoPlugin>& DiscoveredPlugins);
    /** 实例退出时撤销本快照；不卸载资源，资源释放归外层Data租约。 */
    void Reset();
    /** 返回对应插件的独立功能值副本；缺身份返回空数组，不按显示文字匹配。 */
    TArray<FDivineBeastsDemoFeatureEntry> GetFeatures(FName PluginId) const;
    /** 返回主或关联此功能的案例；同图案例均保留。 */
    TArray<FDivineBeastsDemoScenarioDefinition> GetFeatureScenarios(FName FeatureId) const;
    /** 解析正式ID或显式旧路由；一对多返回需要选择，不写OutId。 */
    FGamePlatformResult ResolveIdentity(FName InputId, FName& OutId) const;
    /** 仅查稳定案例身份，返回指针有效期到本快照下次发布/Reset。 */
    const FDivineBeastsDemoScenarioDefinition* FindScenario(FName ScenarioId) const;
    /** 完整功能只读视图，供统计，不受筛选影响。 */
    const TArray<FDivineBeastsDemoFeatureEntry>& GetAllFeatures() const { return Features; }
    /** 完整案例只读视图，供兼容投影和运行预检。 */
    const TArray<FDivineBeastsDemoScenarioDefinition>& GetAllScenarios() const { return Scenarios; }
    /** 完整用例值合同，真实签收记录另存。 */
    const TArray<FDivineBeastsDemoReviewCaseDefinition>& GetReviewCases() const { return Reviews; }
    /** 已完整发布的作者源指纹；未发布为空。 */
    const FString& GetSourceFingerprint() const { return SourceFingerprint; }
    /** 仅表示已有完整结构快照，不表示案例可运行。 */
    bool IsPublished() const { return bPublished; }
private:
    /** 全部值由作者定义复制，仅在完整验证成功时同时替换。 */
    TArray<FDivineBeastsDemoFeatureEntry> Features;
    TArray<FDivineBeastsDemoScenarioDefinition> Scenarios;
    TArray<FDivineBeastsDemoReviewCaseDefinition> Reviews;
    TArray<FDivineBeastsDemoLegacyMapRoute> LegacyRoutes;
    FString SourceFingerprint;
    bool bPublished = false;
};
}
