using UnrealBuildTool;

public class GamePlatformInteraction : ModuleRules
{
    public GamePlatformInteraction(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformGameplay",
            "DeveloperSettings"
        });
    }
}