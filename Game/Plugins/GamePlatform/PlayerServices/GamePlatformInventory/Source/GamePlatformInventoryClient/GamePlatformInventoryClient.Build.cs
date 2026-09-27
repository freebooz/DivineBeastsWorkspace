using UnrealBuildTool;

public class GamePlatformInventoryClient : ModuleRules
{
    public GamePlatformInventoryClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "Json"
        });
    }
}