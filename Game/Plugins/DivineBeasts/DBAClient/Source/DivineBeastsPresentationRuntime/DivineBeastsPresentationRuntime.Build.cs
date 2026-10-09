// 本文件属于DivineBeasts项目层 DivineBeastsPresentationRuntime，负责真实模块依赖/编译装配；不创建运行状态。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
using UnrealBuildTool;

public class DivineBeastsPresentationRuntime : ModuleRules
{
    public DivineBeastsPresentationRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "GameplayTags",
            "DivineBeastsRuntime",
            "GamePlatformPresentationCore"
        });
        // 目录/预载逻辑ID校验直接消费平台身份合同。
        PrivateDependencyModuleNames.Add("GamePlatformCore");
    }
}
