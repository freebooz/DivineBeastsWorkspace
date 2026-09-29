using UnrealBuildTool;

public class GamePlatformOnlineClient : ModuleRules
{
    public GamePlatformOnlineClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 公开客户端子系统直接暴露 Online 公共配置/请求类型，因此必须直接依赖公共 Online 模块。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformOnline"
        });

        // HTTP/JSON 只属于客户端私有传输实现，不泄露到平台公共契约，也不会进入 Dedicated Server 目标。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP",
            "Json"
        });
    }
}