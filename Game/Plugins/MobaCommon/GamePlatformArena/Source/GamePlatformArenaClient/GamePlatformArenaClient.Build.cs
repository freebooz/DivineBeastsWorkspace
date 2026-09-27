using UnrealBuildTool;

public class GamePlatformArenaClient : ModuleRules
{
    public GamePlatformArenaClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 客户端竞技适配依赖双端竞技契约，不引用服务端独占模块。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 本模块公开类型直接继承 UMG/CommonUI 控件，必须显式链接其实现，不能依赖平台UI的传递依赖。
            "UMG",
            "CommonUI",
            "GamePlatformMobaCore",
            "GamePlatformArena",
            // MOBA通用竞技UI只依赖平台UI抽象，不反向依赖任何具体游戏项目。
            "GamePlatformUIClient"
        });
    }
}
