using UnrealBuildTool;

// Lobby／OpenWorld／Village／MainArena共用同一服务器目标；角色由部署配置选择，不为每个角色创建目标。
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
