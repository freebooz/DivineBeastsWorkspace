// 项目竞技权威适配模块：公开契约依赖下层Arena/角色/Data，私有准入结果直接链接平台Core。
// 只在Server/Editor装配，不带公共客户端或表现模块；生命周期所有权由各GameMode组合持有。
using UnrealBuildTool;

public class DivineBeastsArenaServer : ModuleRules
{
    public DivineBeastsArenaServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "GamePlatformCore", "GamePlatformAbilitySystem", "GamePlatformCombat", "GamePlatformGameplay" });
        // 死亡桥直接调用ASC/效果规格；保持私有GAS依赖，不依靠平台适配模块间接提供引擎符号。
        PrivateDependencyModuleNames.Add("GameplayAbilities");
        // 死亡桥回归使用中立五模式定义；其真实实现归MobaData，与公开准入合同的MobaCore职责不同。
        PrivateDependencyModuleNames.Add("GamePlatformMobaData");

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
            // 公开预热租约类型来自Data，真实加载在cpp通过该服务申请。
            "GamePlatformData",
            "GamePlatformArena",
            // 公开准入扩展使用Assignment等MOBA合同，Editor DLL须直接声明其数据类型实现模块。
            "GamePlatformMobaCore",
            "GamePlatformArenaServer"
        });
    }
}
