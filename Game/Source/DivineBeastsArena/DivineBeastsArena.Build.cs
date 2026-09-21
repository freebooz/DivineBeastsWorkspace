using UnrealBuildTool;

// 薄主模块仅装配已经写入的Core/Data/Flow；公开探针继承Data，Flow仅供私有项目协调使用。
public class DivineBeastsArena : ModuleRules
{
    public DivineBeastsArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData" });
        PrivateDependencyModuleNames.AddRange(new[] { "GamePlatformApplicationFlow", "AssetRegistry" });
    }
}
