using UnrealBuildTool;

public class GamePlatformLobbyServer : ModuleRules
{
    public GamePlatformLobbyServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}