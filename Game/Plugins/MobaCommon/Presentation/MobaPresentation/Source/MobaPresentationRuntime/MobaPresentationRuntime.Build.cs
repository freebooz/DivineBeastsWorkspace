using UnrealBuildTool;

public class MobaPresentationRuntime : ModuleRules
{
    public MobaPresentationRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformPresentationCore"
        });
    }
}