using UnrealBuildTool;

public class GamePlatformCameraClient : ModuleRules
{
    public GamePlatformCameraClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 仅客户端CameraShake，不依赖游戏权威Combat、项目玩法或UI资源。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}