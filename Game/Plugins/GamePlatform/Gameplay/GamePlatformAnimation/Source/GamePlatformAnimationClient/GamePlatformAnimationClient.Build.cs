using UnrealBuildTool;

public class GamePlatformAnimationClient : ModuleRules
{
    public GamePlatformAnimationClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 仅客户端视觉动画与LocalPlayer定时器；不依赖Gameplay权威模块。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}