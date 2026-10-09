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
            "GamePlatformUIClient",
            // 当前公开战斗反馈映射直接暴露 FGamePlatformCombatEvent，
            // 背包和任务页面的公开头同样直接暴露平台客户端快照类型，
            // 因此这些模块都是调用方编译公开接口所需的真实 Public 依赖。
            "GamePlatformCombat",
            "GamePlatformInventoryClient",
            "GamePlatformQuest",
            "GamePlatformQuestClient"
        });

        // ApplicationFlow 只在本模块 Private Adapter（私有适配器）中消费；
        // 公开 UI 类型不暴露流程头文件，避免把项目流程依赖扩散给所有 UI 消费者。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsApplicationFlowClient",
            // 仅Private鼠标预览适配使用EKeys，不把输入按键依赖扩散给公开UI契约。
            "InputCore",
            // 玩家状态 ViewModel 只在 Private 实现中订阅平台 ASC/项目 Momentum AttributeSet；公开 UI 契约不泄漏 Gameplay 类型。
            "GameplayAbilities",
            "GamePlatformAbilitySystem",
            "DivineBeastsCharactersRuntime",
            // 角色选择/创建页只通过项目表现子系统驱动三维预览，不直接加载Mesh或地图资产。
            "DivineBeastsPresentationClient"
        });
    }
}
