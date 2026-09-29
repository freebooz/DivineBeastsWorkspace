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
            // 服务器组合层只负责把环境身份/凭据注入平台Telemetry，不复制遥测实现。
            "GamePlatformTelemetry"
        });
    }
}
