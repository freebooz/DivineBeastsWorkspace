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
            "DivineBeastsRuntime",
            "GamePlatformApplicationFlow",
            "GamePlatformOnlineClient",
            "GamePlatformSession",
            "GamePlatformLoading",
            "GamePlatformCharacter"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP",
            "Json",
            "JsonUtilities"
        });
    }
}
