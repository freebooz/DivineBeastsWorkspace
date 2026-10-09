// MOBA服务器/编辑器模块：公开竞技契约与私有已验证连接适配，依赖低层Server；不带客户端表现或项目内容。
using UnrealBuildTool;

public class GamePlatformArenaServer : ModuleRules
{
    public GamePlatformArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 实现只读消费平台已验证连接事件；不引入客户端Online/HTTP认证替代品。
        PrivateDependencyModuleNames.Add("GamePlatformServer");
        // 服务端竞技适配依赖双端竞技契约，不引用客户端表现模块。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "JsonUtilities", "GamePlatformMobaCore", "GamePlatformMobaData", "GamePlatformArena" });
    }
}