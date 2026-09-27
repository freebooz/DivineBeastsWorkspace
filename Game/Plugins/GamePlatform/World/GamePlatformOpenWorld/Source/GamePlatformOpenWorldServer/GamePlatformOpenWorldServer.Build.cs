using UnrealBuildTool;

public class GamePlatformOpenWorldServer : ModuleRules
{
    public GamePlatformOpenWorldServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}