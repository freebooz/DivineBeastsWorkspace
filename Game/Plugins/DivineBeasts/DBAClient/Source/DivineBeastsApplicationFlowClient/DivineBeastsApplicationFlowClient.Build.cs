using UnrealBuildTool;

public class DivineBeastsApplicationFlowClient : ModuleRules
{
    public DivineBeastsApplicationFlowClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            // 项目流程直接使用 FGamePlatformId / FGamePlatformResult 等平台核心契约，
            // 必须声明真实直接依赖，禁止依赖 GamePlatformData 等模块的传递链接关系。
            "GamePlatformCore",
            "DivineBeastsRuntime",
            "GamePlatformApplicationFlow",
            "GamePlatformOnlineClient",
            "GamePlatformSession",
            "GamePlatformLoading",
            "GamePlatformData",
            "DBAWorldsRuntime",
            "GamePlatformCharacter"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "JsonUtilities",
            "GamePlatformGameplay", "GamePlatformWorld",
            // Telemetry只用于客户端组合装配，平台遥测本身不反向依赖Online/ApplicationFlow。
            "GamePlatformTelemetry"
        });
    }
}
