// MOBA双端竞技模块依赖规则：公开契约来自下层Core/Data，目标隔离见插件描述；不链接项目或客户端私有实现。
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