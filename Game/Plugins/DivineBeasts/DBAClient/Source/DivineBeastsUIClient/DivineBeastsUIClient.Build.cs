using UnrealBuildTool;

/// <summary>
/// DivineBeastsUIClient（神兽联盟用户界面客户端模块）构建规则。
/// 本模块只负责项目层 UI 基类、ViewModel、路由和客户端界面适配，不承载服务器权威逻辑。
/// </summary>

public class DivineBeastsUIClient : ModuleRules
{
    public DivineBeastsUIClient(ReadOnlyTargetRules Target) : base(Target)
    {
        // 使用显式或共享 PCH，降低大量 UI 头文件在增量编译中的重复解析开销。
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "Slate",
            "SlateCore",
            "CommonUI",
            "CommonInput",
            "GamePlatformUIClient",
            // 公开UI类持有Data值租约，调用方编译这些头需要平台Data公开合同。
            "GamePlatformData",
            // 公共小队视图只暴露平台普通队伍/请求值合同，不包含竞技语义或传输实现。
            "GamePlatformOnline",
            // 设置页公开快照合同来自现有平台设备设置服务；不另存一份设备配置。
            "GamePlatformSettingsClient",
            // 当前公开战斗反馈映射直接暴露 FGamePlatformCombatEvent，
            // 背包和任务页面的公开头同样直接暴露平台客户端快照类型，
            // 因此这些模块都是调用方编译公开接口所需的真实 Public 依赖。
            "GamePlatformCombat",
            "GamePlatformInventoryClient",
            "GamePlatformQuest",
            "GamePlatformQuestClient"
        });

        // ApplicationFlow 只在本模块 Private Adapter（私有适配器）中消费；
        // 公开 UI 类型不暴露流程头文件，避免把项目流程依赖扩散给所有 UI 消费者。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DivineBeastsApplicationFlowClient",
            // 开发快捷登录仅私有解析进程环境JSON，公开UI合同不暴露口令或JSON对象。
            "Json",
            // 用户点击复制公开玩家ID时调用原生剪贴板；不处理登录凭据或后台自动复制。
            "ApplicationCore",
            // 私有命令桥复用唯一认证、同源传输、预算与取消，不另建HTTP执行器。
            "GamePlatformOnlineClient",
            // 私有ESC接线只消费项目世界控制器公开事件，不让双端世界依赖客户端UI。
            "DBAWorldsRuntime",
            // 世界HUD按项目唯一体验/服务器角色目录选择；不依赖竞技或在UI重复维护角色身份。
            "DivineBeastsRuntime",
            // 仅Private鼠标预览适配使用EKeys，不把输入按键依赖扩散给公开UI契约。
            "InputCore",
            // 玩家状态 ViewModel 只在 Private 实现中订阅平台 ASC/项目 Momentum AttributeSet；公开 UI 契约不泄漏 Gameplay 类型。
            "GameplayAbilities",
            "GamePlatformAbilitySystem",
            // 技能栏仅订阅公开玩家生命周期与中立玩法资格事件，不拥有Active权威状态。
            "GamePlatformGameplay",
            "DivineBeastsCharactersRuntime",
            // 技能栏仅读取项目双端已授权技能快照和本地表现主资产，不取得 GAS 权威写权限。
            "DivineBeastsAbilitiesRuntime",
            "GamePlatformCore",
            // 角色选择/创建页只通过项目表现子系统驱动三维预览，不直接加载Mesh或地图资产。
            "DivineBeastsPresentationClient"
        });
        if (Target.bBuildEditor)
        {
            // 仅编辑器颜色回归读取真实原图像素和浮点GPU结果；不进入专用服务器或客户端发行闭包。
            PrivateDependencyModuleNames.AddRange(new string[] { "ImageCore", "RenderCore", "RHI" });
            // 编辑器回归检验平台瞬态施法动画的真实Percent曲线；项目运行代码不直接驱动MovieScene。
            PrivateDependencyModuleNames.AddRange(new string[] { "MovieScene", "MovieSceneTracks" });
        }
    }
}
