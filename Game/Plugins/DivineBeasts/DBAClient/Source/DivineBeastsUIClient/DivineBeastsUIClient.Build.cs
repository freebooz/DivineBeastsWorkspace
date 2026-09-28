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
            "GamePlatformUIClient",
            // 项目UI只消费这些平台领域的客户端只读状态/事件，不复制第二套业务模型。
            "GamePlatformCombat",
            "GamePlatformInventoryClient",
            "GamePlatformQuestClient"
        });

        // ApplicationFlow 只在本模块 Private Adapter（私有适配器）中消费；
        // 公开 UI 类型不暴露流程头文件，避免把项目流程依赖扩散给所有 UI 消费者。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsApplicationFlowClient"
        });
    }
}
