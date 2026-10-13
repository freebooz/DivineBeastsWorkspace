using UnrealBuildTool;

/// <summary>
/// DivineBeastsDemoClient（SSLM插件演示客户端模块，保留稳定技术身份）。
/// 仅供开发客户端/编辑器使用，Projects读取真实插件描述、AssetRegistry读取真实开发地图；
/// 不进入Shipping/Test/Server目标，不承载业务认证、网络权威或第二套资产管理器。
/// </summary>
public class DivineBeastsDemoClient : ModuleRules
{
    public DivineBeastsDemoClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 公共UI/演示World头公开持有项目飞行配置及平台数据租约/天气类型，不能依赖私有传递。
        PublicDependencyModuleNames.AddRange(new[] {
            "Core","CoreUObject","Engine","UMG",
            "DivineBeastsPresentationClient","GamePlatformWeatherRuntime","GamePlatformData",
            // 开发目录公开参数/结果使用Core稳定契约，必须直接声明Public依赖。
            "GamePlatformCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] {
            "Projects", "AssetRegistry", "DivineBeastsApplicationFlowClient",
            // 演示控制器必须调用已有VFX/天气/世界公开服务，绝不重建执行器。
            "GamePlatformVFXClient", "DBAWorldsRuntime", "SlateCore"
        });
    }
}
