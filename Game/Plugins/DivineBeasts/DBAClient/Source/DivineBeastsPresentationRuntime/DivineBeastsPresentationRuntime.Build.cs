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
            "Engine", // 项目层只维护DataAsset与软引用定义，不执行特效
            "GameplayTags",
            "DivineBeastsRuntime",
            "GamePlatformData", // 项目反馈目录通过统一主资产租约加载
            "GamePlatformPresentationCore"
        });
    }
}
