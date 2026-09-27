// MOBA核心契约模块构建规则。
using UnrealBuildTool;

public class GamePlatformMobaCore : ModuleRules
{
    public GamePlatformMobaCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 仅复用基础层中性类型，不引入任何生肖、地图或项目规则。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GamePlatformCore" });
    }
}
