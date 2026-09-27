using UnrealBuildTool;

public class GamePlatformArenaClient : ModuleRules
{
    public GamePlatformArenaClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 客户端竞技适配依赖双端竞技契约，不引用服务端独占模块。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GamePlatformMobaCore", "GamePlatformArena" });
    }
}