using UnrealBuildTool;

// 平台客户端水表现适配：仅客户端/Editor加载Niagara与UE5.8 Water；服务器不链接此模块。
public class GamePlatformWaterClient : ModuleRules
{
    public GamePlatformWaterClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GamePlatformWaterRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "Niagara", "Water", "GamePlatformData" });
    }
}
