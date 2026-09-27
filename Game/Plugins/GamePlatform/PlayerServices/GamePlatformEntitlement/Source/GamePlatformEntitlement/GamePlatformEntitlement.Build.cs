using UnrealBuildTool;

public class GamePlatformEntitlement : ModuleRules
{
    public GamePlatformEntitlement(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}