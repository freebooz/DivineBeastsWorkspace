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
            "GamePlatformPresentationClient", "GameplayTags"
        });
    }
}
