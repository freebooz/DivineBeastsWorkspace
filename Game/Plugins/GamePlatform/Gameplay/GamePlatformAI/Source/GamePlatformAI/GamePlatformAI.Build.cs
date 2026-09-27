using UnrealBuildTool;

public class GamePlatformAI : ModuleRules
{
    public GamePlatformAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "GameplayTags",
            "DeveloperSettings"
        });
    }
}