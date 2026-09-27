// MOBA数据定义模块构建规则。
using UnrealBuildTool;

public class GamePlatformMobaData : ModuleRules
{
    public GamePlatformMobaData(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 使用基础数据框架扩展MOBA模式定义，不把竞技规则塞回基础层。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GamePlatformMobaCore", "GamePlatformData" });
    }
}
