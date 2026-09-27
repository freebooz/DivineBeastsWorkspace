using UnrealBuildTool;

public class GamePlatformEquipmentServer : ModuleRules
{
    public GamePlatformEquipmentServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags", "GamePlatformEquipment", "GamePlatformAbilitySystem" });
    }
}