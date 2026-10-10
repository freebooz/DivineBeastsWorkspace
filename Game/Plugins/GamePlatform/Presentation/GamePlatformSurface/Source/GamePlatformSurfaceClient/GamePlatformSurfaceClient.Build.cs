// 本文件属于GamePlatform平台层 GamePlatformSurface，负责真实模块依赖/编译装配；不创建运行状态。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
using UnrealBuildTool;

/// <summary>
/// GamePlatformSurfaceClient（游戏平台环境表面客户端模块）构建规则。
/// 仅承载客户端表面材质状态与渲染参数桥接；Dedicated Server 不应链接本模块。
/// </summary>
public class GamePlatformSurfaceClient : ModuleRules
{
    public GamePlatformSurfaceClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings"
        });
        // MPC普通软资源的持有/取消统一由Data管理；不自建AssetManager或同步加载备用链。
        PrivateDependencyModuleNames.Add("GamePlatformData");
        // 内部资源回执调用Core结果/身份方法；真实Editor DLL需要直接导入库，头可见不代表链接成立。
        PrivateDependencyModuleNames.Add("GamePlatformCore");
    }
}
