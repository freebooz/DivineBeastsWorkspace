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
        // 编辑器加载Surface客户端契约、资产挂载点与Editor生成/校验模块，用于真实材质资产制作和回读验证。
        EnablePlugins.Add("GamePlatformSurface");
        // GamePlatformPCG（游戏平台PCG）的模板生成、Commandlet（命令行工具）和DataValidation（数据校验）
        // 需要在Editor Target（编辑器目标）显式装配；不会改变Client/Server目标插件闭包。
        EnablePlugins.Add("GamePlatformPCG");
        // Monolith在编辑器中创建和复核公共UI资产时必须加载真实内容插件挂载点。
        EnablePlugins.Add("DBAUIPack_Core");
        // 编辑器加载公共占位角色源和全部生肖英雄内容包，供资产生成、预览与验证。
        EnablePlugins.Add("DBAContentPack_Common");
        EnablePlugins.Add("DBAHeroPack_Rat");
        EnablePlugins.Add("DBAHeroPack_Ox");
        EnablePlugins.Add("DBAHeroPack_Tiger");
        EnablePlugins.Add("DBAHeroPack_Rabbit");
        EnablePlugins.Add("DBAHeroPack_Dragon");
        EnablePlugins.Add("DBAHeroPack_Snake");
        EnablePlugins.Add("DBAHeroPack_Horse");
        EnablePlugins.Add("DBAHeroPack_Goat");
        EnablePlugins.Add("DBAHeroPack_Monkey");
        EnablePlugins.Add("DBAHeroPack_Rooster");
        EnablePlugins.Add("DBAHeroPack_Dog");
        EnablePlugins.Add("DBAHeroPack_Boar");
    }
}
