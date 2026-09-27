using UnrealBuildTool;

public class GamePlatformTelemetry : ModuleRules
{
    public GamePlatformTelemetry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "TraceLog" });
    }
}