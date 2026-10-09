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
            // 反射生成的项目Widget虚表直接引用引擎UI符号，不能只借平台头文件可见性。
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
            "DivineBeastsApplicationFlowClient"
        });
    }
}
