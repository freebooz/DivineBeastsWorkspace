using UnrealBuildTool;

public class GamePlatformVillageClient : ModuleRules
{
    public GamePlatformVillageClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}