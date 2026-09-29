using UnrealBuildTool;

/// <summary>
/// GamePlatformSettingsServer（游戏平台设置服务器模块）构建规则。
/// 只读取服务器INI/环境变量/命令行并向Runtime提供分层值，不引入任何客户端表现依赖。
/// </summary>
public class GamePlatformSettingsServer : ModuleRules
{
    public GamePlatformSettingsServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformCore",
            "GamePlatformSettingsRuntime"
        });
    }
}
