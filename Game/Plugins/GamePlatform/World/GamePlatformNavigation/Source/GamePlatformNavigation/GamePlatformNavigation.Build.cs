using UnrealBuildTool;

public class GamePlatformNavigation : ModuleRules
{
    public GamePlatformNavigation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "DeveloperSettings",
            "GamePlatformCore",
            "GamePlatformData"
        });
    }
}