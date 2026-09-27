using UnrealBuildTool;

public class GamePlatformEntitlementClient : ModuleRules
{
    public GamePlatformEntitlementClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "GamePlatformEntitlement" });
    }
}