using UnrealBuildTool;

// 仅编辑器创作与验证；不向运行模块传播UnrealEd或资产写入能力。
public class GamePlatformPCGEditor : ModuleRules
{
    public GamePlatformPCGEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry", "Projects",
            "PCG", "GamePlatformPCG", "GamePlatformCore", "GamePlatformData"
        });
    }
}
