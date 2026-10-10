using UnrealBuildTool;

// 双端共享天气契约、权威调度和网络复制；不得引用纯客户端渲染和音效模块。
public class GamePlatformWeatherRuntime : ModuleRules
{
    public GamePlatformWeatherRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "NetCore", "GamePlatformCore", "GamePlatformData" });
    }
}
