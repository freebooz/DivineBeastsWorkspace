using UnrealBuildTool;

public class GamePlatformLiveOpsClient : ModuleRules
{
    public GamePlatformLiveOpsClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json" });
    }
}