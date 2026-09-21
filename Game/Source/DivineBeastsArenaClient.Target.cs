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
    }
}
