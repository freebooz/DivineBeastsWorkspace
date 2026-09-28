using UnrealBuildTool;

// 唯一正式编辑器目标；不将开发场景设为生产默认地图。
public class DivineBeastsArenaEditorTarget : TargetRules
{
    public DivineBeastsArenaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        // 临时用于本次 Monolith MCP 编辑器二进制构建：绕过当前引擎 HTTP 私有测试源码编译错误。
        // 构建完成后必须立即删除，正式 Editor Target 仍保留默认自动化测试策略。
        bForceDisableAutomationTests = true;
        ExtraModuleNames.Add("DivineBeastsArena");
        // 编辑器需要加载DBAClient反射类型与Content，才能生成/验证真实Application Flow DataAsset。
        EnablePlugins.Add("DBAClient");
    }
}
