using UnrealBuildTool;

public class DivineBeastsArenaServer : ModuleRules
{
    public DivineBeastsArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsArenaRuntime",
            "DivineBeastsCharactersRuntime",
            // 竞技出生复用项目技能装配角色；服务端只包含双端安全的玩法模块。
            "DivineBeastsAbilitiesRuntime",
            "GamePlatformCharacter",
            "GamePlatformArena",
            "GamePlatformArenaServer"
        });
    }
}
