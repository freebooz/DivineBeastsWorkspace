using UnrealBuildTool;

public class GamePlatformAnimationClient : ModuleRules
{
    public GamePlatformAnimationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}