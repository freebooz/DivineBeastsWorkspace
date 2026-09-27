using UnrealBuildTool;

public class GamePlatformEquipmentClient : ModuleRules
{
    public GamePlatformEquipmentClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GamePlatformEquipment", "GamePlatformData", "GameplayTags" });
    }
}