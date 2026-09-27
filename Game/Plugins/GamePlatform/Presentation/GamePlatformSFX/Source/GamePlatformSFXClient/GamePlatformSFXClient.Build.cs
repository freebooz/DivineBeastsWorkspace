using UnrealBuildTool;

public class GamePlatformSFXClient : ModuleRules
{
    public GamePlatformSFXClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}