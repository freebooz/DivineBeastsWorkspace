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
            "Json"
        });
    }
}
