using UnrealBuildTool;

public class GamePlatformAIServer : ModuleRules
{
    public GamePlatformAIServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 本模块公开/实现直接消费FGamePlatformResult，DLL须直接链接其Core所有者，不能依赖Data间接可见。
            "GamePlatformCore",
            "AIModule",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "GamePlatformAI",
            "GamePlatformData",
            "GamePlatformAbilitySystem",
            "GamePlatformCombat",
            "GamePlatformNavigationServer"
        });
    }
}