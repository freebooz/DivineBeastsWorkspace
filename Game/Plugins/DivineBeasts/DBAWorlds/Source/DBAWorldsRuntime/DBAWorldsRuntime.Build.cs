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
            // 项目世界定义的公开字段直接使用FGamePlatformId/FGamePlatformResult，必须显式链接其真实所有者。
            "GamePlatformCore",
            // UDivineBeastsWorldDefinition继承Data层定义基类，链接其虚函数实现不能依赖World的传递依赖。
            "GamePlatformData",
            "GamePlatformWorld"
        });
    }
}
