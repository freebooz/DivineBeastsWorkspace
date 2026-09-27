using UnrealBuildTool;

// 真实定义与地图校验只在编辑器/编辑器命令行执行；运行模块不反向依赖本模块。
public class GamePlatformWorldEditor : ModuleRules
{
    public GamePlatformWorldEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData", "GamePlatformWorld",
            "AssetRegistry", "DataValidation", "UnrealEd"
        });
    }
}
