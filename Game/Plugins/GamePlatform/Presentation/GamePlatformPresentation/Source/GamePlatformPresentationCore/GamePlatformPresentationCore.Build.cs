using UnrealBuildTool;

public class GamePlatformPresentationCore : ModuleRules
{
    public GamePlatformPresentationCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 配置资源DataAsset只提供平台中立数据；不加载具体VFX/SFX资源。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayTags" });
    }
}
