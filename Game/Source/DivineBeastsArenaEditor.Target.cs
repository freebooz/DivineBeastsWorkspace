using UnrealBuildTool;

// 唯一正式编辑器目标；不将开发场景设为生产默认地图。
public class DivineBeastsArenaEditorTarget : TargetRules
{
    public DivineBeastsArenaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("DivineBeastsArena");
    }
}
