using UnrealBuildTool;

public class GamePlatformSaveClient : ModuleRules
{
    public GamePlatformSaveClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}