using UnrealBuildTool;

public class DivineBeastsPresentationClient : ModuleRules
{
    public DivineBeastsPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsRuntime",
            "DivineBeastsPresentationRuntime",
            "GamePlatformPresentationCore",
            "GamePlatformPresentationClient"
        });

        // 角色外观Profile只在客户端软加载普通表现资源；GamePlatformData提供统一加载入口。
        // DivineBeastsCharactersRuntime仅用于读取项目角色状态组件并订阅Ready变化，依赖方向保持项目表现层 -> 项目玩法层。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "GamePlatformData",
            "DivineBeastsCharactersRuntime"
        });

    }
}
