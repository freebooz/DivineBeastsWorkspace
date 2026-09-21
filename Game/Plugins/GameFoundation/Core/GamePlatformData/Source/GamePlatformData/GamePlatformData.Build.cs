using UnrealBuildTool;

// 双端数据契约依赖平台核心；资产注册表仅由内部发现与验证使用。
public class GamePlatformData : ModuleRules
{
    public GamePlatformData(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "AssetRegistry" });
    }
}
