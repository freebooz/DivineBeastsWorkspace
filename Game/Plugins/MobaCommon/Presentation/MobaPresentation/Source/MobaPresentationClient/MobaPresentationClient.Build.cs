using UnrealBuildTool;

public class MobaPresentationClient : ModuleRules
{
    public MobaPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "MobaPresentationRuntime",
            "GamePlatformPresentationCore",
            "GamePlatformPresentationClient",
            "GamePlatformMobaCore",
            "GamePlatformArena",
            "GamePlatformCombat"
        });
    }
}