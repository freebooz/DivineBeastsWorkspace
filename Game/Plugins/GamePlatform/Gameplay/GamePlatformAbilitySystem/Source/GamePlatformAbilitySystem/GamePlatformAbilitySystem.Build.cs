using UnrealBuildTool;

// 唯一双端模块；Gameplay/Character/Input和业务准入只在项目组合根通过公开接口连接。
public class GamePlatformAbilitySystem : ModuleRules
{
    public GamePlatformAbilitySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags", "GameplayTasks",
            "GamePlatformCore", "GamePlatformData"
        });
    }
}
