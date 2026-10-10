# DBAArena（神兽联盟可选竞技插件）

正式位置：`Game/Plugins/DivineBeasts/DBAArena/`。本插件承接原本分散在DBAGameplay、DBAClient和DBAServer中的项目竞技适配，保留既有模块、反射类型、导出宏和五种竞技模式身份；不复制通用竞技机制。

| 模块 | 目标 | 职责 |
| --- | --- | --- |
| DivineBeastsArenaRuntime | 双端／编辑器 | 项目竞技模式、资格与规则配置 |
| DivineBeastsArenaClient | Client／Editor | 匹配请求、公开流程扩展、赛后返回入口 |
| DivineBeastsArenaServer | Server／Editor | MainArena项目权威适配 |

## 依赖与装配

- 公共依赖为DBAGameplay和MobaCommon/GamePlatformArena。项目公共玩法、世界、客户端和服务器插件不反向依赖本插件。
- 客户端模块在私有构建依赖中使用DBAClient的`IDivineBeastsApplicationFlowExtension`与公开流程服务；既有按GameInstance注册／注销机制保留，不创建全局桥接器或第二套流程执行器。插件对DBAClient的引用只允许Client／Editor目标。
- Server目标不启用DBAClient，不编译DivineBeastsArenaClient。三角色仍共用一个Server Target；运行角色为MainArena时使用竞技扩展，不能按1v1至5v5再拆程序。
- MobaPresentation是独立MOBA语义插件，按客户端产品组合显式选择；公共项目表现不能硬依赖它，本次不伪称新增了未实现的表现接线。
- 主工程现有Foundation验证装配未被擅自改为自动启动全部业务子系统。关闭竞技验证使用同一正式工程的插件选择，不创建新DevHost；完整UE启用、编译、启动验收仍需实际工具链。

## 生命周期与内容

竞技客户端注册公开流程扩展，反初始化时注销、释放扩展引用及世界状态。服务调用、资产失败与角色就绪沿用既有明确错误路径；不因目录迁移增加固定成功或模拟准入。

项目竞技场地图、美术及专属内容的目标所有者为`ContentPacks/Worlds/DBAWorldPack_MainArena`。本插件只保留代码与必要项目模式定义，不复制地图或通用VFX执行器。内容包尚未交付，不创建假资产。

## 2026-10-09 战斗打击反馈竞技组合根

- `DivineBeastsArenaClient/Private/Feedback/DivineBeastsArenaCombatFeedbackClientSubsystem.h/.cpp`：真实LocalPlayer客户端组合根，事件驱动按权威HeroDefinitionId＋SourceAbilityId选择项目Catalog，利用统一GamePlatformData主资产租约异步加载Profile与Client Bundle，再注入MobaPresentation的中立可选Resolver。首击资源尚未加载时回退平台默认反馈，不调用LoadSynchronous。
- `DivineBeastsArenaClient/Private/Feedback/DivineBeastsArenaCombatFeedbackSettings.h`：仅客户端可配置CatalogDefinitionId（目录稳定主资产ID）。默认空，编辑器未创建/校验资产前保持无配置状态；不虚构演示技能ID或.uasset。
- 租约采用Instance期限但由当前世界和LocalPlayer明确持有，WorldCleanup主动释放、请求代次防止回调串世界；失败目录在同一世界不重复申请。单人及拆分屏幕场景通过独立LocalPlayer隔离；仍需真实UE测试验证。
- 项目公开DBAClient不依赖Moba或竞技，新增Moba依赖仅属于本插件ClientOnly模块；Server模块不引入MobaPresentationClient和纯表现资源。真实资产、完整技能映射、Client/Server Cook及双客户端手感测试待完成。

- Profile在Catalog主资产异步成功后只按当前本地玩家OwnerOnly（仅拥有者）已授予技能快照中的真实技能ID预热；收到尚未预加载的远端技能首次确认命中时按需异步申请，最多保留64份配置。不再遍历Catalog整体预热十二生肖技能，技能/角色代次更换时释放旧租约；技能键不匹配时严禁复用其他英雄的VFX/SFX定义。仅真实ArenaGameState世界激活，普通登录、Village和OpenWorld不因此加载资源。
- 早期UE编译仅取得UHT生成成功，C++构建曾停在本机UBA执行器且被主动停止；该历史记录不能作为当前构建结果。UE5.8本分支`-NoUBA`只关闭Detour，仍会选用UBA执行器；需要排查运行端编译代理后再进行真实客户端构建、专服和资源Cook。


## 2026-10-09 已授权技能预热与资源验收边界

- 游戏状态订阅采用UE5.8 World.GameStateSetEvent，避免GameState复制晚于PostLoadMap时遗漏竞技初始化；本地Controller.Pawn变化及OwnerOnly技能授权快照更新后，仅预热可信HeroDefinitionId、AvatarGeneration和已授予AbilityId匹配的Profile，不再无差别加载全部生肖配置。
- 当前LocalPlayer换英雄或地图时，按原有服务释放本组合根持有的技能Profile租约，及时取消订阅。MobaPresentation只消费低层已加载数据与逻辑VFX/SFX ID，不了解神兽项目资产；当前缺失资源时使用通用默认反馈。
- 首批丑牛/寅虎/卯兔资源检查见 Docs/Implementation/CombatFeedbackAssetGapInventory_20261009.md；外观/技能UI文件不等于Niagara/声音/受击Montage/Profile真实交付。运行中通过客户端控制台gp.Combat.HitstopOverrideFrames测试0/3/6帧，在UE编译和手工运行验收前只能视为已写入功能。

2026-10-10进一步实证：竞技反馈客户端组合根`DivineBeastsArenaCombatFeedbackClientSubsystem.cpp`、项目UI映射`DivineBeastsCombatUIFeedbackLibrary.cpp`及授权、Catalog、UI测试源码均经正式UE5.8 UBT调用MSVC单文件编译，退出码0；不代表最新客户端DLL完成链接或编辑器已经加载新反射类型。真实Catalog、Profile和伤害浮字WBP尚无完整保存/回读。构建和内容门禁见`Docs/Implementation/CombatFeedbackBuildRecovery_20261010.md`及`Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py`。

## 验证

- `Tests/Architecture/DBAPluginConsolidation.Tests.ps1`：模块唯一归属与声明。
- `Tests/Architecture/PluginCompositionAudit.Tests.ps1`：禁用竞技后的公共闭包、竞技Server不引入客户端、循环与隐藏依赖负例。
- `Tests/Architecture/ValidateDesignBaseline.ps1`：46个代码／机制插件＋登记内容插件。
- UE测试源码仍位于各模块的`Private/Tests`。结构检查不是UE编译、Cook、网络或五模式运行验收；本轮证据见总体实施规划。

2026-10-09 联机验证前修复：竞技反馈组合根的两个include曾拼接在同一行，引发C4067；现已分行并补充本地玩家/世界租约边界说明。受影响Editor模块重新编译退出0；Client、Server、Cook和双客户端准入结果分别记录，不能由该结果推断。

本次完整Native构建证据：Client退出0见Saved/Validation/FoundationM0/0d516f9c-7d9c-4a49-b509-167c307aec0e/Build/Client；Server退出0见Saved/Validation/FoundationM0/8e56f639-5f43-4097-9d05-8ad14d337cbc/Build/Server。它们不代表Cook、真实WorldReady或功能体验验收通过。
