using UnrealBuildTool;

public class GamePlatformVillage : ModuleRules
{
    public GamePlatformVillage(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}