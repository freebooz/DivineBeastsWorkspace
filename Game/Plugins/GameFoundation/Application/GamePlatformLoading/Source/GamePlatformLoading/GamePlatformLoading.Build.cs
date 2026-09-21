using UnrealBuildTool;

// 单双端运行模块；不依赖Flow/Session客户端、UMG、Niagara或项目资源。
public class GamePlatformLoading : ModuleRules
{
    public GamePlatformLoading(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData" });
    }
}
