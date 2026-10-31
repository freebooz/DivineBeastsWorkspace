using UnrealBuildTool;

// 编辑器水域审查：只读查询重复水域ID、非法深度和流速，不在编辑器启动时创建关卡。
public class GamePlatformWaterEditor : ModuleRules
{
    public GamePlatformWaterEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "GamePlatformWaterRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "AssetRegistry", "Water", "GeometryCore" });
    }
}
