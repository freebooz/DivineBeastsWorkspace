# DBAWorlds（神兽联盟项目世界插件）

正式位置：`Game/Plugins/DivineBeasts/DBAWorlds/`。插件拥有项目世界定义类型与Shared角色/体验的一致性约束，不强制依赖竞技。实际地图、区域定义实例、程序化世界结果与专属资源按ContentPacks规划归三个世界内容包，由UE资产工具交付，不在本插件复制第二份地图。

`DBAWorldsRuntime`派生平台`UGamePlatformWorldDefinition`，校验项目服务器角色、默认体验和可选竞技模式映射；它不分配服务器、不加载/Travel地图、不引入网络凭据，也不代替平台World/Data模块的运行时生命周期。

2026-10-10 PCG三层接线：项目世界定义额外登记`EnvironmentPCGProfileIds（世界PCG配置引用）`与`PCGBakeManifestIds（静态烘焙清单引用）`，必须列入`GamePlatformData.RequiredDefinitions（平台必需定义依赖）`；跨世界采集/门状态仍是服务器玩法与存档职责，PCG只提供候选稳定锚点。`GamePlatformPCGEditor（平台PCG编辑器模块）`可通过`-BlueprintRoot=/DBAWorldPack_Village/PCG/Blueprints/`生成11类真实放置器Blueprint（蓝图）；但在项目UE5.8构建/编辑器模块恢复并成功执行前，仅是源码创作入口，并非已交付蓝图或地图。

环境表面材质机制归第一层`GamePlatformSurface`：DBAWorlds不依赖其ClientOnly实现，也不在Runtime复制雪／苔藓／湿润／积水算法。具体`MI_DBA_*`材质实例、项目纹理和世界场景资产归对应`DBAWorldPack_*`；客户端世界表现适配可在项目客户端／内容装配层把天气或世界表现事实提交给Surface，Dedicated Server继续只消费服务器安全的世界／玩法Definition。

2026-10-10天气系统一期：现有`ADivineBeastsWorldGameMode`仅在权威世界`BeginPlay`激活`GamePlatformWeatherWorldSubsystem`，默认晴天，不在登录前端部署；`InitialWeather`可通过项目GameMode蓝图默认值调整，`bEnableWeatherSchedule`和`WeatherSchedule`启用合法天气周期。Runtime只依赖`GamePlatformWeatherRuntime`，不访问客户端Niagara/SFX/Surface，客户端天气模块订阅网络快照。若需要正式项目世界专属天气Definition实例，必须由GamePlatformData有效租约加载并在第三层内容包创建真实UE资产，不允许硬引用不存在的天气蓝图。

天气预设的正式消费入口已加至`ADivineBeastsWorldGameMode::InitialWeatherPresetId`：为空时使用可编辑InitialWeather；填写`db.weather.clear@1`、`db.weather.lightrain@1`等合法逻辑ID时，先完成权威世界初始化，再经GamePlatformData的World期限异步Definition租约加载真实天气资产。回执必须核对完整Lease身份与代次，成功后应用目标数值并释放，失败保留原天气。EndPlay主动释放待完成租约，旧世界回调不修改新世界。如果启用自动天气调度，则自动调度优先，不并发加载初始预设。

本轮新增项目天气审核Actor`ADivineBeastsWeatherReviewController`（蓝图可派生，具备运行期ApplyReviewWeather/ReadCurrentWeather）；只允许权威世界显式执行，默认不自动改天气，且不复制或新建WeatherReplicator。真正审核蓝图的编辑器生成入口为`Tools/Unreal/Weather/AuthorWeatherBlueprintAssets.py`，目标位于`/Game/Development/Weather/Blueprints`，不是正式Village地图的一部分。所需8份项目天气Definition资产也由同一UE Editor脚本创建，数据只有天气标量，不包含纯客户端Niagara/SFX硬引用。

当前工程已有引擎生成的前端与Village地图、世界定义及角色体验定义，实际资源归对应内容包。2026-10-09已执行Client、Server完整原生构建及项目Editor模块构建；这些结果不代替当前版本Cook/Stage、真实网络准入与双客户端人工行走验收，具体边界见下节和独立验证记录。

2026-09-27修正UE测试中遗留的四角色正向夹具：大厅和旧大厅兼容体验都归OpenWorld，明确拒绝独立Lobby角色，并检查合法大厅附带竞技模式返回`ArenaModeWorldContextMismatch`。跨语言真源与正向夹具的一致性由`Tests/Architecture/ServerRoleProfiles.Tests.ps1`检查；这不替代尚未执行的UE自动化测试。

### 2026-10-09 新手村准入、角色与本地行走接线

DBAWorldsRuntime 的 DivineBeastsWorldGameMode/DivineBeastsWorldPlayerController 向下继承 GamePlatformGameplay 框架，复用既有准入、体验、出生与复制门禁。DBAServer 将实际 ServerAdmission 的已验证连接投影桥接到门禁，重查 AdmissionId、ConnectionGeneration、SessionEpoch、实例和体验；未准入不出生。平台 World 新增仅C++的 InitializeBoundWorld 接口，原生组合根必须先验证当前连接，平台独立加载真实定义并匹配实际地图、区域与流送事实，投影不能直接返回Ready。

Village 内容包新增共享Pawn及Tutorial体验定义。原有PlayerStart位置保留，增加第二个出生点避免两个客户端互相堵住出生。专用服务器的Pawn定义只引用原生角色类，没有UI、VFX或模型硬引用。客户端只从实际地图、当前受控Pawn、共享Data准备和服务器Active事件形成Loading事实，旧世界回调忽略；委托在EndPlay解绑，不用界面Tick轮询。

角色创建成功仅刷新档案并返回选择页，清除创建侧隐式待选择写入；进入世界必须经过选择命令。传输失败优先展示错误而非残留加载层，客户端默认回退到正式前端地图。

原生ACharacter增加WASD移动、鼠标镜头和空格跳跃，使用引擎CharacterMovement网络复制。客户端Avatar表现提示来自已验证选择，复用Appearance组件异步加载，不把视觉提示写成服务器英雄、技能或持久角色权威身份。当前角色模型及IDLE仍属一期占位内容，不能据此宣称英雄技能、移动动画和正式新手村美术全部完成。

验证记录位于Saved/Validation/VillageFlow/20261009；编译、保存资产及Cook与双客户端人工行走验收分别记录，未取得后者证据前不宣称全链路完成。

### 2026-10-11 世界PC操控与光标

项目世界控制器优先消费原生世界轴：W/S前后，A/D转身，Q/E横移；右键按住时A/D改为横移，左右鼠标同时按住前进，空格跳跃。普通攻击与现有三主动输入保持数字1—4，被动不伪造激活键。默认采用GameAndUI可见自由光标，左键只调整本地观察镜头，右键同时调整角色水平朝向；首次拖动记录视口光标坐标，最后一个鼠标键释放恢复原位和可见状态。滚轮每格50厘米，镜头距离80—1800厘米，保留SpringArm原生碰撞检测。

角色朝向继续使用ControlRotation及CharacterMovement原生移动复制，横移不按运动向量自动转身；独立SpringArm观察角度不进入权威玩法数据。前端预览使用独立GamePlatformCharacterPreviewStage（展示Actor），不由本世界Controller控制，也不改变其转台、镜头或动画。

UI已处理的点击在GameAndUI阶段不会交给世界后备输入；未处理的按钮RMB、禁用按钮LMB也不能开始拖动。首次捕获使用当次Slate命中路径，忽略Enabled过滤以保留禁用控件，必须包含当前游戏实际SViewport且叶子也是该视口；任何UI叶子都保留光标。正式HUD非交互背景必须保持不拦截命中，实际按钮由UI处理点击，不按分辨率或固定HUD矩形猜测范围。已从世界开始的捕获允许第二个鼠标键组成前进，释放不受之后的命中变化阻挡。Slate依赖仅加入Game/Client/Editor目标，不加入Dedicated Server。

菜单由客户端订阅`OnWorldEscapeRequested`，使用`SetWorldGameplayInputSuspended`暂停/恢复本地输入；共享DBAWorldsRuntime不依赖UIClient。菜单暂停、引擎失焦FlushPressedKeys、Pawn变更和EndPlay都清除鼠标按住、待消费移动、跳跃及ASC按住命令。暂停期间UI拥有UIOnly焦点，控制器清理不覆盖该输入模式。该暂停不修改服务器Active或玩家准入。世界控制器只申请/释放自身各一次Move/Look输入锁，幂等事实刷新不会累计；不调用ResetIgnore输入接口清零其他系统的锁，外部Look锁也禁止镜头和键盘转向。

`Private/Gameplay/DivineBeastsWorldControlState.h`为当前Controller的纯鼠标组合策略；`Private/Tests/DivineBeastsWorldControlsTests.cpp`同时支持UE Automation逻辑用例`DivineBeasts.World.Controls.MouseAndMovement`与独立原生策略编译。`Tests/Integration/World/TestWorldControls.ps1`检查键位冲突、清理接线与双端隔离，可通过`-PolicyExecutable`运行当前源码的独立测试产物。已执行MSVC纯策略及静态检查；未由本子任务执行UE构建、视口操作、Cook或联机，输入手感、真实UI命中、焦点和角色网络朝向仍需要对应运行证据。

官方按键参考：[基础移动说明](https://worldofwarcraft.blizzard.com/en-us/news/20151231/welcome-to-world-of-warcraft-basic-movement-and-combat)、[经典版官方手册](https://eu.media.blizzard.com/manuals/wow/eu/classic/wow-classic-manual-enGB.pdf)。鼠标原位恢复和滚轮范围为本项目明确策略，不冒充官方手册中的数值。
