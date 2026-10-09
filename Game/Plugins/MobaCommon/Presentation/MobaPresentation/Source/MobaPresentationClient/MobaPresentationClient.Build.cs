using UnrealBuildTool;

public class MobaPresentationClient : ModuleRules
{
    public MobaPresentationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "MobaPresentationRuntime",
            "GamePlatformPresentationCore",
            "GamePlatformPresentationClient",
            "GamePlatformMobaCore",
            "GamePlatformArena",
            "GamePlatformCombat"
        });

        // MOBA客户端只调用平台视觉局部顿帧；不反向包含项目层资源与逻辑。
        PrivateDependencyModuleNames.AddRange(new string[] { "GamePlatformAnimationClient" });
    }
}