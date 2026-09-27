using UnrealBuildTool;

public class GamePlatformVFXClient : ModuleRules
{
    public GamePlatformVFXClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "Niagara",
            "NiagaraCore",
            "DeveloperSettings",
            "GamePlatformData",
            "GamePlatformPresentationCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "GamePlatformPresentationClient"
        });
    }
}