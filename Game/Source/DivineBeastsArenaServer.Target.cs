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
        // 将专用服务器打包规则身份写入构建收据，供UAT Stage读取；
        // 角色地图仍由各自Cook配置选择，三角色继续共用同一程序。
        CustomConfig = "DedicatedServer";
        // 公共服务器组合负责三类服务器角色的生命周期、Ready与准入，并通过依赖拉入DBAGameplay角色规则。
        EnablePlugins.Add("DBAServer");
        // 仅装配天气共享权威模块；ClientOnly模块不进入Dedicated Server目标。
        EnablePlugins.Add("GamePlatformWeather");
        // MainArena与OpenWorld/Village共用同一Server Target；竞技插件的ServerOnly模块在MainArena配置下提供项目竞技适配，
        // 其DBAClient依赖带Client/Editor目标白名单，不会进入Dedicated Server产物。
        EnablePlugins.Add("DBAArena");
        // Village权威地图由第三层纯内容包持有；Dedicated Server必须挂载同一地图身份以执行BeginPlay/碰撞/导航与就绪校验。
        EnablePlugins.Add("DBAWorldPack_Village");
    }
}
