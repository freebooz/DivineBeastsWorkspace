using UnrealBuildTool;

public class DivineBeastsArenaRuntime : ModuleRules
{
    public DivineBeastsArenaRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformMobaCore",
            "GamePlatformMobaData",
            "GamePlatformArena",
            "DivineBeastsRuntime",
            "DivineBeastsCharactersRuntime"
        });
    }
}
