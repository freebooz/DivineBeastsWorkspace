using UnrealBuildTool;

// 编辑器水域审查：只读查询重复水域ID、非法深度和流速，不在编辑器启动时创建关卡。
public class GamePlatformWaterEditor : ModuleRules
{
    public GamePlatformWaterEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "GamePlatformWaterRuntime" });
        // 水体资产、PhysicsVolume及几何审查只在Editor模块装配，服务器沿用权威Runtime边界。
        PrivateDependencyModuleNames.AddRange(new[] { "AssetRegistry", "Water", "GeometryCore" });
    }
}
