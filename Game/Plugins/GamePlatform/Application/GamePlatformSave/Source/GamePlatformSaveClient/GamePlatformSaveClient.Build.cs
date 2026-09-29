using UnrealBuildTool;

/// <summary>
/// GamePlatformSaveClient（游戏平台本地存档客户端模块）构建规则。
/// 该模块只提供客户端非权威本地存档机制，不直接依赖在线、背包、装备、成长、UI或项目层业务模块。
/// </summary>
public class GamePlatformSaveClient : ModuleRules
{
    public GamePlatformSaveClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformCore"
        });
    }
}