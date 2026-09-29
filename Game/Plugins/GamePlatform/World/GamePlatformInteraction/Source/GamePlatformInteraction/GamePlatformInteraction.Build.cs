using UnrealBuildTool;

public class GamePlatformInteraction : ModuleRules
{
    public GamePlatformInteraction(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Public（公开）依赖只保留公开头文件签名真正使用的模块，避免把实现细节向上游传播。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "DeveloperSettings"
        });

        // Private（私有）依赖仅用于运行实现：复制辅助与平台玩法资格 Provider（提供者）。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NetCore",
            "GamePlatformGameplay"
        });
    }
}