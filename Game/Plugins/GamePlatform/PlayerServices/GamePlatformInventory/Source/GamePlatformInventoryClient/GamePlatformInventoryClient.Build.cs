// GamePlatformInventoryClient编译合同：客户端本地玩家投影模块；仅声明实际公开/私有依赖，生命周期和数据所有权由相邻公开契约/README说明。
using UnrealBuildTool;

public class GamePlatformInventoryClient : ModuleRules
{
    public GamePlatformInventoryClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        // Online认证请求通道与JSON只在客户端内部实现使用，不向背包公共契约扩散。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "GamePlatformOnline",
            "GamePlatformOnlineClient",
            "Json"
        });
    }
}