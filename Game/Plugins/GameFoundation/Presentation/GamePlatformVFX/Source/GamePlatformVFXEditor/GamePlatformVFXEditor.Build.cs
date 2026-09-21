using UnrealBuildTool;

public class GamePlatformVFXEditor : ModuleRules
{
    public GamePlatformVFXEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformVFXClient"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "DataValidation",
            "AssetRegistry",
            "Projects"
        });
    }
}
