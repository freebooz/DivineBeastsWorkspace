using UnrealBuildTool;

/// <summary>
/// 平台VFX编辑器模块：校验定义、复合依赖和Niagara制作合同，只消费公开客户端/Data/Core契约。
/// 直接调用Data主资产类型函数须显式链接GamePlatformData；不访问其他模块Private，不启动世界播放或生产连接。
/// </summary>
public class GamePlatformVFXEditor : ModuleRules
{
    public GamePlatformVFXEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GamePlatformVFXClient"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "GamePlatformCore",
            "GamePlatformData",
            "AssetRegistry",
            "AssetTools",
            "PropertyEditor",
            "Niagara",
            "GameplayTags",
            "NiagaraEditor"
        });
    }
}
