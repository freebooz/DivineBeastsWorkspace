using UnrealBuildTool;

/// <summary>
/// GamePlatformUIClient（游戏平台用户界面客户端模块）构建规则。
/// 该模块仅承载客户端/编辑器可用的 CommonUI、输入、自适应和界面生命周期代码。
/// Dedicated Server（专用服务器）由插件 ClientOnly 模块类型排除，不应链接本模块。
/// </summary>

public class GamePlatformUIClient : ModuleRules
{
    public GamePlatformUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        // 使用显式或共享 PCH，减少大型 UE 工程中不必要的重复头文件解析。
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "CommonInput",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformInputClient",
            "GamePlatformPresentationCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });
    }
}