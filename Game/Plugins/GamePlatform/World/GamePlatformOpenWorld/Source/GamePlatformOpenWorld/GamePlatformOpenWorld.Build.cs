using UnrealBuildTool;

public class GamePlatformOpenWorld : ModuleRules
{
    public GamePlatformOpenWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}