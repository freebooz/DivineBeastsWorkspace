using UnrealBuildTool;

public class DivineBeastsCharactersRuntime : ModuleRules
{
    public DivineBeastsCharactersRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Public头公开GameplayTag、平台角色契约及项目身份类型；依赖只保留真实可见边界。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 本模块公开/实现直接消费FGamePlatformResult，DLL须直接链接其Core所有者，不能依赖Data间接可见。
            "GamePlatformCore",
            "GameplayAbilities",
            "GameplayTags",
            "GamePlatformAbilitySystem",
            "GamePlatformCharacter",
            // 角色公开资源状态返回Data结果/租约值；加载和引用所有权仍归平台Data。
            "GamePlatformData",
            "DivineBeastsRuntime"
        });

        // UnrealNetwork仅用于本模块复制实现，不能通过Public依赖向上层传播。
        // GameplayAbilities/GamePlatformAbilitySystem 为 Public：Momentum AttributeSet 的公开头继承平台 AttributeSet 并暴露 GAS 属性类型。
        // Gameplay只用于项目层资格组合，平台AbilitySystem不反向认识Gameplay或生肖。
        PrivateDependencyModuleNames.AddRange(new[] { "NetCore", "GamePlatformGameplay" });
    }
}
