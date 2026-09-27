using UnrealBuildTool;

// 发布端可读定义/清单；运行时纯装饰仍由世界网络模式明确拒绝专服执行。
public class GamePlatformPCG : ModuleRules
{
    public GamePlatformPCG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData", "PCG" });
        PrivateDependencyModuleNames.AddRange(new[] { "GamePlatformWorld" });
    }
}
