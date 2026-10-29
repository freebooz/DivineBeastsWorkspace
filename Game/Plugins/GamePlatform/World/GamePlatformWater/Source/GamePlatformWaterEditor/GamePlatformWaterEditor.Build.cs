using UnrealBuildTool;

// 编辑器水域审查：只读查询重复水域ID、非法深度和流速，不在编辑器启动时创建关卡。
public class GamePlatformWaterEditor : ModuleRules
{
    public GamePlatformWaterEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "GamePlatformWaterRuntime" });
        // 仅Editor审查WaterBodyLake水下材质与PhysicsVolume，Dedicated Server不引入Water渲染模块。
        PrivateDependencyModuleNames.Add("Water");
    }
}
