using UnrealBuildTool;

public class GamePlatformPresentationClient : ModuleRules
{
    public GamePlatformPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GamePlatformPresentationCore"
        });
    }
}