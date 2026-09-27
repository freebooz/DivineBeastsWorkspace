using UnrealBuildTool;

public class GamePlatformCombat : ModuleRules
{
    public GamePlatformCombat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "GamePlatformCore",
            "GamePlatformAbilitySystem",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "PhysicsCore"
        });
    }
}