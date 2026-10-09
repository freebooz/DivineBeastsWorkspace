using UnrealBuildTool;

/// <summary>
/// DivineBeastsAbilitiesRuntime（神兽联盟技能运行模块）。
/// 双端共享只读技能定义、数值校验和服务器授权组件；没有客户端 UI／VFX／SFX 依赖。
/// </summary>
public class DivineBeastsAbilitiesRuntime : ModuleRules
{
    public DivineBeastsAbilitiesRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 公开定义继承平台 Definition，并使用 Gameplay、Combat 与角色的公开类型。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags",
            "GamePlatformCore", "GamePlatformData", "GamePlatformAbilitySystem",
            "GamePlatformCombat", "DivineBeastsCharactersRuntime"
        });
        // NetCore 仅用于本模块拥有者定向复制实现；不将服务器内部入口公开给客户端。
        // 真实角色装配回归直接读取平台GameplayEligibility合同，须直接声明所属模块，不依赖Characters间接链接。
        PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "GamePlatformGameplay" });
    }
}
