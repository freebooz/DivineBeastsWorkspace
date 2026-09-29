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
    }
}
