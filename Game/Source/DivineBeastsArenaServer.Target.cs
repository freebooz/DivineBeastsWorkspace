using UnrealBuildTool;

// OpenWorld／Village／MainArena共用同一Dedicated Server目标；登录大厅属于OpenWorld体验，不创建独立Lobby服务器角色。
public class DivineBeastsArenaServerTarget : TargetRules
{
    public DivineBeastsArenaServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("DivineBeastsArena");
        // 公共服务器组合负责三类服务器角色的生命周期、Ready与准入，并通过依赖拉入DBAGameplay角色规则。
        EnablePlugins.Add("DBAServer");
        // MainArena与OpenWorld/Village共用同一Server Target；竞技插件的ServerOnly模块在MainArena配置下提供项目竞技适配，
        // 其DBAClient依赖带Client/Editor目标白名单，不会进入Dedicated Server产物。
        EnablePlugins.Add("DBAArena");
    }
}
