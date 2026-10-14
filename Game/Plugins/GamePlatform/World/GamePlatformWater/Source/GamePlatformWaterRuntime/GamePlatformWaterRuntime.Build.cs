using UnrealBuildTool;

// 平台水域公共逻辑：服务端仅查询注册的静态水域，不依赖Water/Niagara/Slate/编辑器。
public class GamePlatformWaterRuntime : ModuleRules
{
    public GamePlatformWaterRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
