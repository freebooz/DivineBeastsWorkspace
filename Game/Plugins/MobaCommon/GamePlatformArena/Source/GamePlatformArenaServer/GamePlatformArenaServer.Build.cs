using UnrealBuildTool;

public class GamePlatformArenaServer : ModuleRules
{
    public GamePlatformArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 服务端竞技适配依赖双端竞技契约，不引用客户端表现模块。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "HTTP", "Json", "JsonUtilities", "GamePlatformMobaCore", "GamePlatformMobaData", "GamePlatformArena" });
    }
}