using UnrealBuildTool;

// 双端流程机制：公开类型消费Core契约与Data定义/租约，不链接具体在线、UI或玩法插件。
public class GamePlatformApplicationFlow : ModuleRules
{
    public GamePlatformApplicationFlow(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData" });
    }
}
