using UnrealBuildTool;

// 薄主模块只拥有项目装配，尚未实现的插件不得提前成为构建依赖。
public class DivineBeastsArena : ModuleRules
{
    public DivineBeastsArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
