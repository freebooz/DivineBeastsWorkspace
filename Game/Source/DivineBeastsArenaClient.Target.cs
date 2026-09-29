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
        // 通用环境表面材质属于纯客户端表现能力；显式按Client Target装配，避免Dedicated Server携带材质代码与Content。
        EnablePlugins.Add("GamePlatformSurface");
        // 公共UI二进制资产由第三层纯内容插件拥有；客户端显式启用，服务器目标不携带。
        EnablePlugins.Add("DBAUIPack_Core");
        // 十二生肖角色视觉内容只进入客户端：公共Mannequin源 + 12个独立英雄包。
        // 后期真实生肖替换仍沿用这些插件身份和Profile路径，不修改逻辑HeroDefinitionId。
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
