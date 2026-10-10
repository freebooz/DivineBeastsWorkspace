using UnrealBuildTool;

/// <summary>
/// GamePlatformSettingsEditor（游戏平台设置编辑器校验模块）构建规则。
/// 依赖Runtime公开校验和Core结果值实现；仅编辑器目标，不进入Client/Server Shipping。
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
            "GamePlatformSettingsRuntime",
            // Validator直接消费FGamePlatformResult::IsSuccess与析构，必须链接Core导入库。
            "GamePlatformCore"
        });
    }
}
