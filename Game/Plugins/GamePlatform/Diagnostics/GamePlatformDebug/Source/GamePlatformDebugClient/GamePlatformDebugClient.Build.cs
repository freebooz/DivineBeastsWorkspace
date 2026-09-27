using UnrealBuildTool;

public class GamePlatformDebugClient : ModuleRules
{
    public GamePlatformDebugClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformDebug"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "InputCore",
            "Slate",
            "SlateCore"
        });

        PublicDefinitions.Add(
            Target.Configuration == UnrealTargetConfiguration.Shipping
                ? "GAME_PLATFORM_DEBUG_CLIENT_ENABLED=0"
                : "GAME_PLATFORM_DEBUG_CLIENT_ENABLED=1");
    }
}
