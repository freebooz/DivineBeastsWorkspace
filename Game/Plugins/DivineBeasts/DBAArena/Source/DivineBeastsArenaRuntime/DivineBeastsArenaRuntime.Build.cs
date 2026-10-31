using UnrealBuildTool;

public class DivineBeastsArenaRuntime : ModuleRules
{
    public DivineBeastsArenaRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 公开原生语义标签由真实双端Runtime注册，客户端/服务器使用同一标签表。
            "GameplayTags",
            "GamePlatformMobaCore",
            "GamePlatformMobaData",
            "GamePlatformArena",
            "DivineBeastsRuntime",
            "DivineBeastsCharactersRuntime"
        });
    }
}
