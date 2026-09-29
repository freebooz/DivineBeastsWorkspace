using UnrealBuildTool;

/// <summary>
/// GamePlatformSettingsEditor（游戏平台设置编辑器校验模块）构建规则。
/// 仅依赖Runtime公开验证入口，不进入Client/Server Shipping。
/// </summary>
public class GamePlatformSettingsEditor : ModuleRules
{
    public GamePlatformSettingsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformSettingsRuntime"
        });
    }
}
