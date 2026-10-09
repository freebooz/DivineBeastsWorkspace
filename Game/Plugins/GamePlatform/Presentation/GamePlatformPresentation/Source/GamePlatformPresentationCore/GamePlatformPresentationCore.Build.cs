using UnrealBuildTool;

// 平台双端中立表现契约：向上层公开反馈配置定义，依赖平台核心结果与统一数据契约。
// 不持有世界执行器或项目资产；构建依赖必须同时保证公开头可见和DLL符号可链接。
public class GamePlatformPresentationCore : ModuleRules
{
    public GamePlatformPresentationCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 配置资源DataAsset只提供平台中立数据；不加载具体VFX/SFX资源。
        // 公开验证接口及实现直接使用FGamePlatformResult，不能只通过Data的头文件可见性推断链接依赖。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayTags", "GamePlatformCore", "GamePlatformData" });
    }
}
