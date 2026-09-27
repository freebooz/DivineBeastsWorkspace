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
