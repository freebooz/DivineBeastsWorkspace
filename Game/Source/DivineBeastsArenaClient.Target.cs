using UnrealBuildTool;

// 隔离客户端目标，不能由普通 Game 目标替代。
public class DivineBeastsArenaClientTarget : TargetRules
{
    public DivineBeastsArenaClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("DivineBeastsArena");
        // 正式客户端组合根显式启用DBAClient；其插件依赖会继续拉入Online/Session/Loading/UI等客户端能力。
        // 不在.uproject全局启用，避免Server Target被动携带客户端Runtime组合模块。
        EnablePlugins.Add("DBAClient");
    }
}
