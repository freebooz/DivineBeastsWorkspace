using UnrealBuildTool;

/// <summary>
/// DivineBeastsDemoClient（SSLM插件演示客户端模块，保留稳定技术身份）。
/// 仅供开发客户端/编辑器使用，Projects读取真实插件描述、AssetRegistry读取真实开发地图；
/// 不进入Shipping/Test/Server目标，不承载业务认证、网络权威或第二套资产管理器。
/// </summary>
public class DivineBeastsDemoClient : ModuleRules
{
    public DivineBeastsDemoClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 公共UI/演示World头公开持有项目飞行配置及平台数据租约/天气类型，不能依赖私有传递。
        PublicDependencyModuleNames.AddRange(new[] {
            "Core","CoreUObject","Engine","UMG",
            "DivineBeastsPresentationClient","GamePlatformWeatherRuntime","GamePlatformData",
            // 开发目录公开参数/结果使用Core稳定契约，必须直接声明Public依赖。
            "GamePlatformCore", "DivineBeastsUIClient", "GamePlatformUIClient", "SlateCore",
            // 公开演示Screen继承CommonUI基类，虚表链接必须直接声明所属模块。
            "CommonUI"
        });
        PrivateDependencyModuleNames.AddRange(new[] {
            "Projects", "AssetRegistry", "DivineBeastsApplicationFlowClient",
            // 演示控制器必须调用已有VFX/天气/世界公开服务，绝不重建执行器。
            "GamePlatformVFXClient", "DBAWorldsRuntime", "Json", "JsonUtilities",
            // 仅本开发演示使用既有平台电影摄像机，允许客户端私有光学控制与键盘交互。
            "GamePlatformCameraClient", "CinematicCamera", "InputCore",
            // 私有ListView桥实例化STableViewBase模板，头可见不能替代实际Slate链接依赖。
            "Slate",
            // 独立人工页调用平台现有服务；均为开发模块私有依赖，不公开上层适配类型。
            "GamePlatformApplicationFlow", "GamePlatformInputClient", "GamePlatformLoading",
            "GamePlatformSaveClient", "GamePlatformSettingsRuntime", "GamePlatformSettingsClient",
            "GamePlatformOnline", "GamePlatformOnlineClient", "GamePlatformSession",
            "GamePlatformCommerceUIClient", "GamePlatformEntitlement", "GamePlatformEntitlementClient",
            "GamePlatformEquipment", "GamePlatformEquipmentClient", "GamePlatformInventoryClient",
            "GamePlatformLiveOpsClient", "GamePlatformProgression", "GamePlatformProgressionClient",
            "EnhancedInput", "GameplayTags", "GamePlatformDebug",
            "GamePlatformPresentationCore", "GamePlatformPresentationClient", "GamePlatformSFXClient",
            "GamePlatformSurfaceClient", "PhysicsCore",
            "GamePlatformWorld", "GamePlatformWeatherClient", "GamePlatformWaterRuntime", "GamePlatformWaterClient",
            "GamePlatformPCG", "GamePlatformNavigation", "GamePlatformInteraction", "GamePlatformTelemetry", "PCG",
            "GamePlatformGameplay", "GamePlatformCharacter", "GamePlatformAbilitySystem", "GamePlatformCombat", "GamePlatformAnimationClient", "GamePlatformAI", "GamePlatformQuest", "GamePlatformQuestClient", "GameplayAbilities", "DivineBeastsCharactersRuntime"
        });
        if (Target.Type == TargetType.Editor)
        {
            // 仅Editor私有桥生成待审对象；Client公共头不含Editor/Review类型，更不执行人工决定。
            PrivateDependencyModuleNames.AddRange(new[] { "GamePlatformDeveloperTools", "UnrealEd",
                "GamePlatformServer", "GamePlatformEquipmentServer", "GamePlatformSurfaceEditor", "GamePlatformPCGEditor", "GamePlatformNavigationServer", "NavigationSystem", "AIModule", "GamePlatformAIServer", "GamePlatformQuestServer" });
        }
        if (Target.bWithLiveCoding)
        {
            // 仅用引擎公开接口观察已加载会话状态，不加载/开启/编译；关闭该引擎能力的目标不引入头依赖。
            PrivateIncludePathModuleNames.Add("LiveCoding");
        }
    }
}
