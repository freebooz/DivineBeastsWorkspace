using UnrealBuildTool;

public class GamePlatformOnline : ModuleRules
{
    public GamePlatformOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 公开门面使用CoreUObject值类型，并将GamePlatformCore结果类型暴露给调用方，因此必须声明公开模块依赖。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "GamePlatformCore" });
    }
}
