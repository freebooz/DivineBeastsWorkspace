using UnrealBuildTool;

// 公开契约使用原生输入类型及Data定义；服务器目标由描述和项目依赖条件共同排除。
public class GamePlatformInputClient : ModuleRules
{
    public GamePlatformInputClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "EnhancedInput", "InputCore", "GameplayTags", "GamePlatformCore", "GamePlatformData" });
        PrivateDependencyModuleNames.AddRange(new[] { "Json", "JsonUtilities" });
    }
}
