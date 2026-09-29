using UnrealBuildTool;

/// <summary>
/// GamePlatformSettingsClient（游戏平台设置客户端适配模块）构建规则。
/// 只依赖Runtime/Core与Engine，不直接依赖Input、Camera、SFX、UI、Online或项目层。
/// </summary>
public class GamePlatformSettingsClient : ModuleRules
{
    public GamePlatformSettingsClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformCore",
            "GamePlatformSettingsRuntime"
        });
    }
}
