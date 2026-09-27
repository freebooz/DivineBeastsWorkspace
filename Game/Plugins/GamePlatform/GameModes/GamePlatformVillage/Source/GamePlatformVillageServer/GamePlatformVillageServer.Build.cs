using UnrealBuildTool;

public class GamePlatformVillageServer : ModuleRules
{
    public GamePlatformVillageServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}