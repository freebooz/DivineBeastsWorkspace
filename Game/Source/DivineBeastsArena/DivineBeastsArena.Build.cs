using UnrealBuildTool;

// 薄主模块仅装配已经写入的Core/Data/Flow；公开探针继承Data，Flow仅供私有项目协调使用。
public class DivineBeastsArena : ModuleRules
{
    public DivineBeastsArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformCore", "GamePlatformData" });
        // 主模块私有 Bootstrap 直接使用 Online 门面与 JSON 解析，必须声明真实直接依赖，禁止依赖 Engine/其他模块的传递关系。
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "GamePlatformApplicationFlow",
            "GamePlatformLoading",
            "GamePlatformWorld",
            "GamePlatformPCG",
            "GamePlatformOnline",
            "AssetRegistry",
            "Json"
        });
    }
}
