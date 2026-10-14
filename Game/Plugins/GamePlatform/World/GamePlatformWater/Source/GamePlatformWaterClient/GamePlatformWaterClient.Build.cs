using UnrealBuildTool;

// 水客户端只提交平台中立表现语义请求，Niagara由GamePlatformVFX唯一执行；Water仅用于水面审查。
public class GamePlatformWaterClient : ModuleRules
{
    public GamePlatformWaterClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformWaterRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "Water", "GamePlatformPresentationCore", "GamePlatformPresentationClient", "GameplayTags" });
    }
}
