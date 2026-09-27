using UnrealBuildTool;

// 当前仅编译真实状态内核；缺失Online公开接口时不声明虚假依赖或加载玩家服务。
public class GamePlatformSession : ModuleRules
{
    public GamePlatformSession(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("Core");
        if (Target.Type != TargetType.Client && Target.Type != TargetType.Editor)
        {
            throw new BuildException("GamePlatformSession仅允许Client或Editor目标；不得链接专用服务器。");
        }
    }
}
