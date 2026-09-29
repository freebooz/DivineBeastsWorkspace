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