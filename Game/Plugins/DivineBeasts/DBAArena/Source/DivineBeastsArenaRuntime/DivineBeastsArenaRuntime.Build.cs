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
        // 同项目共享操控，Village自身不依赖竞技，平台和MOBA均不认识项目组件。
        PrivateDependencyModuleNames.Add("DBAWorldsRuntime");
    }
}
