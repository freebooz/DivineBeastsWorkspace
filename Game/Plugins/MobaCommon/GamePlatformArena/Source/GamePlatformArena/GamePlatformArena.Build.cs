using UnrealBuildTool;

public class GamePlatformArena : ModuleRules
{
    public GamePlatformArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 通用竞技规则依赖MOBA契约和数据，不依赖神兽联盟项目层。
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NetCore", "GamePlatformMobaCore", "GamePlatformMobaData" });
    }
}