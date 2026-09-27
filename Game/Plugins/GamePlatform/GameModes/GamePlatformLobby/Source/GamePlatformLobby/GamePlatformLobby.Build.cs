using UnrealBuildTool;

public class GamePlatformLobby : ModuleRules
{
    public GamePlatformLobby(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}