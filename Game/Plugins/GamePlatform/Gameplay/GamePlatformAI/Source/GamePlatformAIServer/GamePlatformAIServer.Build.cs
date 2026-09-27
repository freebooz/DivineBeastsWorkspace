using UnrealBuildTool;

public class GamePlatformAIServer : ModuleRules
{
    public GamePlatformAIServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AIModule",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "GamePlatformAI",
            "GamePlatformData",
            "GamePlatformAbilitySystem",
            "GamePlatformCombat",
            "GamePlatformNavigationServer"
        });
    }
}