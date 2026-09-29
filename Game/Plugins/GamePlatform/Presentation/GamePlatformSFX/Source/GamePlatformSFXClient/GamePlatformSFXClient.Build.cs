using UnrealBuildTool;

/// <summary>
/// GamePlatformSFXClient（游戏平台音效客户端模块）构建规则。
/// 只承载客户端音效机制；Dedicated Server 不应链接本模块。
/// </summary>
public class GamePlatformSFXClient : ModuleRules
{
    public GamePlatformSFXClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "GamePlatformCore",
            "GamePlatformData",
            "GamePlatformPresentationCore"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "GamePlatformPresentationClient"
        });
    }
}