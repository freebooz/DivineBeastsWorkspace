// GamePlatformProgression编译合同：共享平台纯值/规则定义模块；仅声明实际公开/私有依赖，生命周期和数据所有权由相邻公开契约/README说明。
using UnrealBuildTool;

public class GamePlatformProgression : ModuleRules
{
    public GamePlatformProgression(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
    }
}