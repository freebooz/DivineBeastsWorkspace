using UnrealBuildTool;

public class GamePlatformCameraClient : ModuleRules
{
    public GamePlatformCameraClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}