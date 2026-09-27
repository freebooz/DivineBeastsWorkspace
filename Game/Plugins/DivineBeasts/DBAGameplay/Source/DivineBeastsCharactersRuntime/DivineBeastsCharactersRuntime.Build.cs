using UnrealBuildTool;

public class DivineBeastsCharactersRuntime : ModuleRules
{
    public DivineBeastsCharactersRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformCharacter",
            "DivineBeastsRuntime"
        });
    }
}
