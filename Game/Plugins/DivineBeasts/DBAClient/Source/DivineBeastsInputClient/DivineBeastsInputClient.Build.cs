using UnrealBuildTool;

/// <summary>
/// DivineBeastsInputClient（神兽联盟输入客户端模块）。
/// 只声明项目输入语义、项目Profile校验和平台输入事件桥；
/// 不复制Enhanced Input、租约、重绑定或服务器权威逻辑。
/// </summary>
public class DivineBeastsInputClient : ModuleRules
{
    public DivineBeastsInputClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformInputClient"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // EnhancedInput只在项目适配实现中用于确认当前Controller/Pawn的真实接收组件，不泄漏到公开契约。
            "EnhancedInput",
            // 项目输入只通过平台公开AbilityInputReceiver合同驱动GAS，不直接访问私有ASC实现。
            "GamePlatformAbilitySystem",
            // 命中表现结束时恢复输入缓冲，依赖平台ClientOnly模块而非MOBA/项目竞技。
            "GamePlatformAnimationClient",
            "DeveloperSettings"
        });
    }
}
