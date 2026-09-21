using UnrealBuildTool;

// 仅编辑器/命令行数据验证；运行模块不反向链接编辑器。
public class GamePlatformDataEditor : ModuleRules
{
    public GamePlatformDataEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AssetRegistry", "GamePlatformCore", "GamePlatformData", "DataValidation", "UnrealEd" });
    }
}
