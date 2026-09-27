using UnrealBuildTool;

public class GamePlatformCommerceUIClient : ModuleRules
{
    public GamePlatformCommerceUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "Json",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "GamePlatformUIClient"
        });
    }
}