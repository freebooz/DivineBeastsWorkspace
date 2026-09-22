using UnrealBuildTool;

// 双端共享运行模块；在线、会话、加载、输入、PCG、UI及项目实现只允许在组合根适配。
public class GamePlatformGameplay : ModuleRules
{
    public GamePlatformGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformWorld"
        });
    }
}
