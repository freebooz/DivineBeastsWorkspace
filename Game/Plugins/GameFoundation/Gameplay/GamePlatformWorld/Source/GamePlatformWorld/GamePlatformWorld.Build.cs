using UnrealBuildTool;
// 双端运行；不链接Online、UI或Session的客户端模块。Loading反向不依赖World。
public class GamePlatformWorld : ModuleRules
{
    public GamePlatformWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GameplayTags", "GamePlatformCore", "GamePlatformData", "GamePlatformLoading" });
    }
}
