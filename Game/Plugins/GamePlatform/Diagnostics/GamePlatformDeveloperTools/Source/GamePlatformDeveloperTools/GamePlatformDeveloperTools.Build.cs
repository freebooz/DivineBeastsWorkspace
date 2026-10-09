using UnrealBuildTool;
// 平台Editor验证模块：只依赖基础定义契约与引擎工具，按需审计资产，不进入客户端/服务器运行产物。

public class GamePlatformDeveloperTools : ModuleRules
{
    public GamePlatformDeveloperTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // GamePlatformDeveloperTools（游戏平台开发者工具）是纯Editor/CI模块。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "GamePlatformData",
            "GamePlatformCore",
            "AssetRegistry",
            "ContentBrowser",
            "DataValidation",
            "DeveloperSettings",
            "EditorSubsystem",
            "GameplayTags",
            "Json",
            "JsonUtilities",
            "Projects",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd"
        });

        PublicDefinitions.Add("GAME_PLATFORM_DEVELOPER_TOOLS_EDITOR_ONLY=1");
    }
}
