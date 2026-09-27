using UnrealBuildTool;

public class GamePlatformEquipment : ModuleRules
{
    public GamePlatformEquipment(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NetCore", "GameplayTags" });
    }
}