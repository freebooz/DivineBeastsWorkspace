using UnrealBuildTool;

// 纯客户端表现桥接：只消费天气权威快照，不包含调度和任何服务器业务。
public class GamePlatformWeatherClient : ModuleRules
{
    public GamePlatformWeatherClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformWeatherRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "GamePlatformSurfaceClient", "GamePlatformPresentationCore",
            "GamePlatformGameplay", // 消费既有GameState引擎时钟校正通知，不依赖竞技或项目
            "GamePlatformPresentationClient", "GameplayTags"
        });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "GamePlatformSurfaceEditor" });
    }
}
