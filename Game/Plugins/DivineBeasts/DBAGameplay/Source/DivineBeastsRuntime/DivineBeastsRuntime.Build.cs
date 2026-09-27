using System.IO;
using UnrealBuildTool;

public class DivineBeastsRuntime : ModuleRules
{
    public DivineBeastsRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "GamePlatformCore"
        });

        string WorkspaceRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "../../../../../../"));
        string GeneratedRoot = Path.Combine(
            WorkspaceRoot,
            "Shared/Generated/Cpp/Games/DivineBeasts");

        if (!Directory.Exists(GeneratedRoot))
        {
            throw new BuildException(
                "DivineBeasts generated C++ catalog is missing. " +
                "运行Backend/internal/tools/contractcodegen并检查Shared生成目录后再编译。");
        }

        PublicSystemIncludePaths.Add(GeneratedRoot);
        PublicDefinitions.Add("DIVINE_BEASTS_GENERATED_CATALOG=1");
    }
}
