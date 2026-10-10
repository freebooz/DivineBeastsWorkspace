using UnrealBuildTool;

public class DivineBeastsArenaClient : ModuleRules
{
    public DivineBeastsArenaClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 公共竞技HUD等公开UI类型继承UMG和CommonUI基类；
            // 创建真实Widget、反射类及虚函数链接必须直接依赖对应引擎模块。
            "UMG",
            "CommonUI",
            "DivineBeastsArenaRuntime",
            "GamePlatformArena",
            "GamePlatformArenaClient",
            // 竞技项目UI公开类型直接继承平台UI/MOBA UI基类，因此公开声明依赖。
            "GamePlatformUIClient"
        });

        // 仅私有接线消费公共流程扩展接口；竞技公开契约不暴露流程实现，公共流程不反向依赖竞技。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsApplicationFlowClient",
            // 只在客户端组合根接入已授权角色身份、数据资产租约和MOBA中立反馈回调。
            "DivineBeastsCharactersRuntime",
            "DivineBeastsAbilitiesRuntime", // 只读取拥有者已授予技能快照，绝不授予/激活技能。
            "DivineBeastsPresentationRuntime",
            // 竞技客户端只复用项目战斗UI DTO→平台反馈的现有适配，不新造Widget或复制UI管理器。
            "DivineBeastsUIClient",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformCombat",
            "GamePlatformPresentationCore",
            "MobaPresentationClient",
            "DeveloperSettings"
        });
    }
}
