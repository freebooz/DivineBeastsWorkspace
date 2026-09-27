using UnrealBuildTool;

public class GamePlatformQuestClient : ModuleRules
{
    public GamePlatformQuestClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformQuest"
        });
    }
}