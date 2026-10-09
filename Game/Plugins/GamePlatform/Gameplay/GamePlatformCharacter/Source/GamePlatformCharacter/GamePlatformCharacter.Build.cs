using UnrealBuildTool;

public class GamePlatformCharacter : ModuleRules
{
    public GamePlatformCharacter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 本模块公开/实现直接消费FGamePlatformResult，DLL须直接链接其Core所有者，不能依赖Data间接可见。
            "GamePlatformCore",
            // 公开英雄加载合同返回Data租约；声明真实Public依赖，保持插件唯一资源所有者。
            "GamePlatformData"
        });

    }
}
