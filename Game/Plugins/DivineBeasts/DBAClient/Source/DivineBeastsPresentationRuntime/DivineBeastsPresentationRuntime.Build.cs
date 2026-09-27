using UnrealBuildTool;

public class DivineBeastsPresentationRuntime : ModuleRules
{
    public DivineBeastsPresentationRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "GameplayTags",
            "DivineBeastsRuntime",
            "GamePlatformPresentationCore"
        });
    }
}
