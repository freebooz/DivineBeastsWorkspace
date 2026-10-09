using UnrealBuildTool;

// 项目表现语义与定义模块：供项目客户端和编辑器消费，不负责播放或保存权威战斗状态。
// 定义验证直接使用平台身份和结果值，必须声明其所属模块的真实链接依赖。
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
            "GamePlatformCore", // 目录验证直接调用FGamePlatformId及FGamePlatformResult的DLL导出方法
            "DivineBeastsRuntime",
            "GamePlatformData", // 项目反馈目录通过统一主资产租约加载
            "GamePlatformPresentationCore"
        });
    }
}
