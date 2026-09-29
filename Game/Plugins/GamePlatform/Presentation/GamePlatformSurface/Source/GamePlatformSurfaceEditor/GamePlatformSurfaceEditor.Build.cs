using UnrealBuildTool;

/// <summary>
/// GamePlatformSurfaceEditor（游戏平台环境表面编辑器模块）构建规则。
/// 负责真实UE资产生成、契约校验与自动化测试；不会进入Client或Dedicated Server产物。
/// </summary>
public class GamePlatformSurfaceEditor : ModuleRules
{
    public GamePlatformSurfaceEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AssetRegistry",
            "Projects",
            "GamePlatformSurfaceClient"
        });
    }
}
