// 本文件属于DivineBeasts项目层 DivineBeastsPresentationClient，负责真实模块依赖/编译装配；不创建运行状态。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// 项目客户端表现装配：公开头含Data值租约，公开声明Data；角色外观只读取下层契约。
using UnrealBuildTool;

public class DivineBeastsPresentationClient : ModuleRules
{
    public DivineBeastsPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 公开回执使用平台结果值；显式链接其实现，不能仅依赖Data头文件的传递可见性。
            "GamePlatformCore",
            "DivineBeastsRuntime",
            "DivineBeastsPresentationRuntime",
            "GamePlatformPresentationCore",
            "GamePlatformPresentationClient",
            "GamePlatformData"
        });

        // 角色外观Profile只在客户端软加载普通表现资源；GamePlatformData提供统一加载入口。
        // DivineBeastsCharactersRuntime仅用于读取项目角色状态组件并订阅Ready变化，依赖方向保持项目表现层 -> 项目玩法层。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsCharactersRuntime",
            // 本地玩家表现编排按世界天气快照订阅，只有第三层依赖平台天气Runtime；不引入服务器私有或天气客户端执行器。
            "GamePlatformWeatherRuntime",
            // 第三层在真实内容包激活后，仅回调平台天气客户端重新发布当前视觉；不访问Niagara/SFX执行器私有实现。
            "GamePlatformWeatherClient"
        });

    }
}
