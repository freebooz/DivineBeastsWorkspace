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
        // 编辑器需要加载DBAClient反射类型与Content，才能生成/验证真实Application Flow DataAsset。
        EnablePlugins.Add("DBAClient");
        // Monolith在编辑器中创建和复核公共UI资产时必须加载真实内容插件挂载点。
        EnablePlugins.Add("DBAUIPack_Core");
    }
}
