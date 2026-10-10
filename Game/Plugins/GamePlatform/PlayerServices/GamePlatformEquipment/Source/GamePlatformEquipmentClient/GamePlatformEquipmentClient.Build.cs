// GamePlatformEquipmentClient编译合同：客户端本地玩家投影模块；仅声明实际公开/私有依赖，生命周期和数据所有权由相邻公开契约/README说明。
using UnrealBuildTool;

public class GamePlatformEquipmentClient : ModuleRules
{
    public GamePlatformEquipmentClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GamePlatformEquipment", "GamePlatformData", "GameplayTags" });
    }
}