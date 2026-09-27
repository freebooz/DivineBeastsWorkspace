using UnrealBuildTool;

// 当前仅编译真实状态内核；缺失Online公开接口时不声明虚假依赖或加载玩家服务。
public class GamePlatformSession : ModuleRules
{
    public GamePlatformSession(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 公开客户端子系统需要UObject/GameInstance生命周期与平台统一结果类型；
        // 私有纯状态内核仍不依赖Engine、网络或项目模块。
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformCore"
        });
        if (Target.Type != TargetType.Client && Target.Type != TargetType.Editor)
        {
            throw new BuildException("GamePlatformSession仅允许Client或Editor目标；不得链接专用服务器。");
        }
    }
}
