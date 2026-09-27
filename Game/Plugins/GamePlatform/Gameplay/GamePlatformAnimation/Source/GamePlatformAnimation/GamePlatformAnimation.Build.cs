using UnrealBuildTool;

public class GamePlatformAnimation : ModuleRules
{
    public GamePlatformAnimation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}