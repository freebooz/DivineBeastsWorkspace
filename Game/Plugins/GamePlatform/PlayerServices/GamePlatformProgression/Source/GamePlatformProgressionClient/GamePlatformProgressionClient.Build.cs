using UnrealBuildTool;

public class GamePlatformProgressionClient : ModuleRules
{
    public GamePlatformProgressionClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "GamePlatformProgression" });
    }
}