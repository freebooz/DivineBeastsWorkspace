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
            "GamePlatformPresentationCore"
        });
    }
}
