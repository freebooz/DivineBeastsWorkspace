using UnrealBuildTool;

public class GamePlatformCameraClient : ModuleRules
{
    public GamePlatformCameraClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 仅客户端相机表现；CinematicCamera是引擎电影镜头模块，不包含项目依赖和服务器玩法。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "CinematicCamera" });
    }
}
