using UnrealBuildTool;

public class GamePlatformQuestServer : ModuleRules
{
    public GamePlatformQuestServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GamePlatformQuest",
            "GamePlatformGameplay"
        });
    }
}