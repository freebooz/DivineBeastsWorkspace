// 项目竞技客户端适配与页面类型；真实公开Widget继承链直接链接UMG/CommonUI及下层竞技UI。
// 仅Client/Editor装配，服务器不得依赖；公共流程扩展仅在私有组合处消费。
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
            // 公开子系统头持有Data租约值并声明Core结果回调，消费者须获得两个真实公开合同。
            "GamePlatformCore",
            "GamePlatformData",
            // 公开竞技Widget继承UMG/CommonUI，反射虚表与实例创建直接链接对应模块，不能只借下层头可见性。
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
            "GamePlatformCombat",
            "GamePlatformPresentationCore",
            "MobaPresentationClient",
            "DeveloperSettings"
        });
    }
}
