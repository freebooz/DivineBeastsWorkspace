using UnrealBuildTool;

public class GamePlatformTelemetry : ModuleRules
{
    public GamePlatformTelemetry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Public 只传播稳定 UE 基础契约；HTTP/Json/TraceLog 均属于遥测内部实现，避免上层插件被迫承担传递编译依赖。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP",
            "Json",
            "TraceLog"
        });
    }
}