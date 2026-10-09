// 客户端领域适配：公开构造接收Online实例，HTTP/Token仅由Online拥有，专服不得装配。
using UnrealBuildTool;

public class GamePlatformProgressionClient : ModuleRules
{
    public GamePlatformProgressionClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Online终态回归直接调用FGamePlatformResult非内联函数；必须链接定义模块，不能借Online或UI传递依赖。
        PrivateDependencyModuleNames.Add("GamePlatformCore");
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "GamePlatformOnlineClient", "CoreUObject", "Engine", "Json", "GamePlatformProgression" });
    }
}