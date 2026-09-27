using UnrealBuildTool;

public class GamePlatformLocalizationClient : ModuleRules
{
    public GamePlatformLocalizationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}