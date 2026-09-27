using UnrealBuildTool;

public class GamePlatformSettingsClient : ModuleRules
{
    public GamePlatformSettingsClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}