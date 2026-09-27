using UnrealBuildTool;

public class DivineBeastsArenaServer : ModuleRules
{
    public DivineBeastsArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsArenaRuntime",
            "DivineBeastsCharactersRuntime",
            "GamePlatformArena",
            "GamePlatformArenaServer"
        });
    }
}
