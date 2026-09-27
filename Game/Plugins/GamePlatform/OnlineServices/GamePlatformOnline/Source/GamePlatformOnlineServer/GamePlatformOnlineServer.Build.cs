using UnrealBuildTool;

public class GamePlatformOnlineServer : ModuleRules
{
    public GamePlatformOnlineServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}