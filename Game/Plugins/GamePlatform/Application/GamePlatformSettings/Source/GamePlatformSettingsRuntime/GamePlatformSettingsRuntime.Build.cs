using UnrealBuildTool;

/// <summary>
/// GamePlatformSettingsRuntime（游戏平台设置运行时核心）构建规则。
/// 只依赖基础引擎与GamePlatformCore，不依赖Input/UI/Camera/SFX/Online或项目层。
/// </summary>
public class GamePlatformSettingsRuntime : ModuleRules
{
    public GamePlatformSettingsRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "GamePlatformCore"
        });
    }
}
