using UnrealBuildTool;

public class GamePlatformQuest : ModuleRules
{
    public GamePlatformQuest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "GameplayTags",
            "DeveloperSettings",
            "GamePlatformCore",
            "GamePlatformData"
        });
    }
}