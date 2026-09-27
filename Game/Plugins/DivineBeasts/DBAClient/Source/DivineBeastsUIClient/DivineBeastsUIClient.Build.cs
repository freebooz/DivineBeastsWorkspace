using UnrealBuildTool;

public class DivineBeastsUIClient : ModuleRules
{
    public DivineBeastsUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "CommonInput",
            "DivineBeastsRuntime",
            "GamePlatformUIClient"
        });
    }
}
