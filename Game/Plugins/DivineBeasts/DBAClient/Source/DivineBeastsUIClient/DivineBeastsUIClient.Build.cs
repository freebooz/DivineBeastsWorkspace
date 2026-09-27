using UnrealBuildTool;

/// <summary>
/// DivineBeastsUIClient（神兽联盟用户界面客户端模块）构建规则。
/// 本模块只负责项目层 UI 基类、ViewModel、路由和客户端界面适配，不承载服务器权威逻辑。
/// </summary>

public class DivineBeastsUIClient : ModuleRules
{
    public DivineBeastsUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        // 使用显式或共享 PCH，降低大量 UI 头文件在增量编译中的重复解析开销。
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
            "DivineBeastsRuntime",
            "GamePlatformUIClient"
        });
    }
}
