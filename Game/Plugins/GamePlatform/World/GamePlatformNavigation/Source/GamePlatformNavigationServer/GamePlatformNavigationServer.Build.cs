using UnrealBuildTool;

public class GamePlatformNavigationServer : ModuleRules
{
    public GamePlatformNavigationServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NavigationSystem",
            "AIModule",
            "GameplayTags",
            "GamePlatformNavigation",
            "GamePlatformWorld"
        });
    }
}