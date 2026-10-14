using UnrealBuildTool;

/// <summary>第三层双端世界定义、服务器装配与本地操控策略；依赖平台公开契约，不执行全局实例分配，不依赖客户端UI。</summary>
public class DBAWorldsRuntime : ModuleRules
{
    public DBAWorldsRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsRuntime",
            // 项目世界定义的公开字段直接使用FGamePlatformId/FGamePlatformResult，必须显式链接其真实所有者。
            "GamePlatformCore",
            // UDivineBeastsWorldDefinition继承Data层定义基类，链接其虚函数实现不能依赖World的传递依赖。
            "GamePlatformData",
            "GamePlatformWorld",
            // 第三层世界服务器组合使用平台天气权威入口，不携带客户端表面/音效实现。
            "GamePlatformWeatherRuntime",
            // 客户端和专服共用的权威水域查询，仅有CPU体积及流速规则。
            "GamePlatformWaterRuntime",
            // 共享项目GameMode/Controller继承平台公开门禁，不链接服务器私有准入实现。
            "GamePlatformGameplay"
        });
        // 项目本地准备必须读取角色定义Ready并订阅事件；不链接服务器私有准入或客户端表现模块。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsCharactersRuntime",
            // 世界输入清理仅通过平台公开ASC撤销按住命令；不引入客户端UI、Niagara或网络权威替代。
            "GamePlatformAbilitySystem",
            // 本地焦点及服务器交互RPC由平台唯一组件承载，项目只选择输入方式。
            "GamePlatformInteraction"
        });
        // 鼠标命中检验只用于有视口的目标；Dedicated Server不链接Slate或任何项目UI客户端。
        if (Target.Type != TargetType.Server)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
        }
    }
}
