using UnrealBuildTool;

public class GamePlatformServer : ModuleRules
{
    // GamePlatformServer（游戏平台服务器控制面模块）只暴露中立生命周期契约；HTTP/JSON保持私有实现依赖。
    public GamePlatformServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP",
            "Json"
        });
    }
}
