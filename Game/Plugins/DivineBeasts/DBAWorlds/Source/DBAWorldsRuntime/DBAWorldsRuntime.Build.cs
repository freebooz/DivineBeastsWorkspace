using UnrealBuildTool;

/// <summary>项目世界定义校验模块；只依赖项目目录与通用World契约，不执行地图分配或加载。</summary>
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
            // 共享项目GameMode/Controller继承平台公开门禁，不链接服务器私有准入实现。
            "GamePlatformGameplay"
        });
        // 项目本地准备必须读取角色定义Ready并订阅事件；不链接服务器私有准入或客户端表现模块。
        PrivateDependencyModuleNames.Add("DivineBeastsCharactersRuntime");
    }
}
