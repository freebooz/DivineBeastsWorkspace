// GamePlatformEquipmentServer编译合同：服务器权威装备模块；仅声明实际公开/私有依赖，生命周期和数据所有权由相邻公开契约/README说明。
using UnrealBuildTool;

public class GamePlatformEquipmentServer : ModuleRules
{
    public GamePlatformEquipmentServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags", "GamePlatformEquipment", "GamePlatformAbilitySystem" });
    }
}