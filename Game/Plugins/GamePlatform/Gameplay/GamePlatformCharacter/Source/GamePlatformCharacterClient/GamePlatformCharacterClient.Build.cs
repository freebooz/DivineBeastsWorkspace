using UnrealBuildTool;

public class GamePlatformCharacterClient : ModuleRules
{
    public GamePlatformCharacterClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core" });
    }
}