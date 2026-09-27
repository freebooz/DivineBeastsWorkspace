using UnrealBuildTool;

public class GamePlatformPresentationCore : ModuleRules
{
    public GamePlatformPresentationCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "GameplayTags" });
    }
}
