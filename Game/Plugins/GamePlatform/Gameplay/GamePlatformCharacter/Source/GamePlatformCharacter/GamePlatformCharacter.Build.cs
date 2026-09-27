using UnrealBuildTool;

public class GamePlatformCharacter : ModuleRules
{
    public GamePlatformCharacter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.Add("GamePlatformData");
    }
}