using UnrealBuildTool;

/// <summary>项目世界定义校验模块；只依赖项目目录与通用World契约，不执行地图分配或加载。</summary>
public class DBAWorldsRuntime : ModuleRules
{
    public DBAWorldsRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsRuntime",
            "GamePlatformWorld"
        });
    }
}
