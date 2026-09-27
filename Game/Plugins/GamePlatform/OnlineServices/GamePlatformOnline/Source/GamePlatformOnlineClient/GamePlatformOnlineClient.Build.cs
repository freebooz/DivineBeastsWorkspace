using UnrealBuildTool;

public class GamePlatformOnlineClient : ModuleRules
{
    public GamePlatformOnlineClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}