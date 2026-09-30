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
            "GameplayAbilities",
            "GameplayTags",
            "GamePlatformAbilitySystem",
            "GamePlatformCharacter",
            "DivineBeastsRuntime"
        });

        // UnrealNetwork仅用于本模块复制实现，不能通过Public依赖向上层传播。
        // GameplayAbilities/GamePlatformAbilitySystem 为 Public：Momentum AttributeSet 的公开头继承平台 AttributeSet 并暴露 GAS 属性类型。
        PrivateDependencyModuleNames.Add("NetCore");
    }
}
