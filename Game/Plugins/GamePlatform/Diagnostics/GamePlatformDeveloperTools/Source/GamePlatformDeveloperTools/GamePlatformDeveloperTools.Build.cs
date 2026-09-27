using UnrealBuildTool;

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