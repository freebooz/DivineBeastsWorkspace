using UnrealBuildTool;

// 双端通用流程机制：公开契约只依赖 UE 基础类型与 GameInstance，不链接具体在线、UI 或玩法插件。
public class GamePlatformApplicationFlow : ModuleRules
{
    public GamePlatformApplicationFlow(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
