using UnrealBuildTool;

public class GamePlatformLobbyClient : ModuleRules
{
    public GamePlatformLobbyClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}