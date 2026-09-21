using UnrealBuildTool;

// OpenWorld／Village／MainArena 共用服务器目标，本批不实现三角色业务。
public class DivineBeastsArenaServerTarget : TargetRules
{
    public DivineBeastsArenaServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("DivineBeastsArena");
    }
}
