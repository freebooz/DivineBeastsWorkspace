using UnrealBuildTool;

public class GamePlatformOnline : ModuleRules
{
    public GamePlatformOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 公共契约只暴露Core/CoreUObject与GamePlatformCore值类型。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "GamePlatformCore"
        });

        // UGameInstance仅用于按实例维护非拥有服务注册表，不引入客户端HTTP实现。
        PrivateDependencyModuleNames.Add("Engine");
    }
}
