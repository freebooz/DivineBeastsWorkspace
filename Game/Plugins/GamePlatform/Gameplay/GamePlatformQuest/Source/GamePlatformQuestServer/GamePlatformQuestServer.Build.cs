using UnrealBuildTool;

public class GamePlatformQuestServer : ModuleRules
{
    public GamePlatformQuestServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            // 服务端任务状态机只消费任务共享契约，不依赖体验/出生等通用玩法运行模块。
            "GamePlatformQuest"
        });
    }
}
