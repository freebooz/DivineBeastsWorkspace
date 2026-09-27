using UnrealBuildTool;

public class DivineBeastsPresentationClient : ModuleRules
{
    public DivineBeastsPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsRuntime",
            "DivineBeastsPresentationRuntime",
            "GamePlatformPresentationCore",
            "GamePlatformPresentationClient"
        });
    }
}
