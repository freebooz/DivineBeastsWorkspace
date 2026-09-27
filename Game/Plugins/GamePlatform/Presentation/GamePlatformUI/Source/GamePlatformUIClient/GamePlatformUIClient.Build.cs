using UnrealBuildTool;

public class GamePlatformUIClient : ModuleRules
{
    public GamePlatformUIClient(ReadOnlyTargetRules Target) : base(Target)
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
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformInputClient",
            "GamePlatformPresentationCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });
    }
}