using UnrealBuildTool;

/// <summary>中立值类型与字节摘要工具；全端可用，不引入Engine、Data或流程服务。</summary>
public class GamePlatformCore : ModuleRules
{
    /// <summary>按宿主目标构建同一实现；测试入口分别由UE自动化宏与独立CMake目标控制。</summary>
    public GamePlatformCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject" });
        // 私有使用引擎自带静态库；公共头不暴露OpenSSL，不引入运行时插件。
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
