// 客户端领域适配：公开构造接收Online实例，HTTP/Token仅由Online拥有，专服不得装配。
using UnrealBuildTool;

public class GamePlatformCommerceUIClient : ModuleRules
{
    public GamePlatformCommerceUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        // HTTP仅提供公开URL编码器，领域请求创建与认证由Online负责。
        PrivateDependencyModuleNames.Add("HTTP");
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Online终态回归直接调用FGamePlatformResult非内联函数；必须链接定义模块，不能借Online或UI传递依赖。
        PrivateDependencyModuleNames.Add("GamePlatformCore");
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "GamePlatformOnlineClient",
            "CoreUObject",
            "Engine",

            "Json",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "GamePlatformUIClient"
        });
    }
}
