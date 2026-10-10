// 项目第三层Server/Editor构建合同：服务器组合根调用平台准入、世界、玩法和数据服务，依赖仅向下。
// 本模块持有当前Boot/Profile的装配状态，不拥有全局分配或客户端资源；凭据由运行环境注入。
using UnrealBuildTool;

// 项目服务器组合模块负责角色Profile和Ready门禁；不承载主竞技场机制或客户端实现。
public class DBAServer : ModuleRules
{
    public DBAServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsRuntime",
            "GamePlatformServer"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            // 可信PlayerData只读查询及项目角色初始化均在服务器私有适配中，不引入客户端Mesh/动画依赖。
            "HTTP", "DivineBeastsCharactersRuntime", "GamePlatformCharacter",
            // 世界准入桥直接构造/解析Core身份与结果，DLL链接必须声明真实库，不能借Data的头可见性。
            "GamePlatformCore",
            // 第三层可信组合根把真实Server准入投影桥接到共享玩法门禁，不复制认证和出生状态机。
            "GamePlatformGameplay", "GamePlatformWorld", "GamePlatformData", "DBAWorldsRuntime",
            // 服务器组合层只负责把环境身份/凭据注入平台Telemetry，不复制遥测实现。
            "GamePlatformTelemetry"
        });
    }
}
