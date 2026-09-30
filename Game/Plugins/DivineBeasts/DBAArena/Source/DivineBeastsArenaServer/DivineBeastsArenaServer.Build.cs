// 项目竞技权威适配模块：公开契约依赖下层Arena/角色/Data，私有准入结果直接链接平台Core。
// 只在Server/Editor装配，不带公共客户端或表现模块；生命周期所有权由各GameMode组合持有。
using UnrealBuildTool;

public class DivineBeastsArenaServer : ModuleRules
{
    public DivineBeastsArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("GamePlatformCore");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DivineBeastsArenaRuntime",
            "DivineBeastsCharactersRuntime",
            "GamePlatformCharacter",
            // 公开预热租约类型来自Data，真实加载在cpp通过该服务申请。
            "GamePlatformData",
            "GamePlatformArena",
            "GamePlatformArenaServer"
        });
    }
}
