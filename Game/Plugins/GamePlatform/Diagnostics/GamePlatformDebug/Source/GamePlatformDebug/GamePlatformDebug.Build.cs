using UnrealBuildTool;

public class GamePlatformDebug : ModuleRules
{
    public GamePlatformDebug(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 按 UE5.8 官方方式配置 Gameplay Debugger（玩法调试器）条件依赖与宏。
        SetupGameplayDebuggerSupport(Target);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NavigationSystem",
            "GameplayAbilities",
            "GameplayTags",
            "GamePlatformCombat",
            "GamePlatformAI",
            "GamePlatformTelemetry"
        });

        bool bDebugAvailable =
            Target.Configuration == UnrealTargetConfiguration.Debug ||
            Target.Configuration == UnrealTargetConfiguration.DebugGame ||
            Target.Configuration == UnrealTargetConfiguration.Development ||
            Target.Configuration == UnrealTargetConfiguration.Test;

        PublicDefinitions.Add(
            bDebugAvailable
                ? "GAME_PLATFORM_DEBUG_ENABLED=1"
                : "GAME_PLATFORM_DEBUG_ENABLED=0");

        PublicDefinitions.Add(
            Target.Configuration == UnrealTargetConfiguration.Test
                ? "GAME_PLATFORM_DEBUG_TEST_BUILD=1"
                : "GAME_PLATFORM_DEBUG_TEST_BUILD=0");
    }
}
