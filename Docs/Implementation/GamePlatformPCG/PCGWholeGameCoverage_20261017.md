# SSLM 全游戏 PCG 覆盖审查与整改计划（2026-10-17）

> 唯一工程：DivineBeastsWorkspace/Game/DivineBeastsArena.uproject。代码与蓝图“存在”不等于实际关卡已生成，不等于生产验收通过。

## 一、当前真实结论

PCG（程序化内容生成）已有GamePlatformPCG Runtime（运行模块）和Editor（编辑器模块）、Schema V1、P0～P9十种稳定原语、空间优先级、世界编排器、环境Definition（定义）与蓝图创作工具。遵守第一层GamePlatform（平台通用）、第三层DivineBeasts（SSLM项目内容）的职责分界，不需要重新建第二套PCG插件。

实地盘点：Foundation（基础验证）12模板、7子图、17金标准定义、3实际生成图、3配置、1测试地图；Village（新手村）11放置器蓝图、4地形蓝图、5地形PCG图、3外围森林候选点图、1正式村庄地图；MainArena（主竞技场）1正式地图。本轮检查的68个文件中68个具有UE真实包头C1-83-2A-9E。这只是文件存在证明；蓝图父类、图连线、Spawner（网格生成器）、注册、地图实例和正式烘焙未由此次磁盘检查确认。

内容包注册表只登记了Village与MainArena，未发现OpenWorld（开放世界）正式内容包。外围雨林目前三张源点图尚未形成经验证的正式植物StaticMeshSpawner图及MeshSet（网格集合）。上述两项为明确覆盖缺口，不能声称完整游戏PCG完成。

## 二、三个世界的覆盖标准

Village：必须在正式L_Village_Start地图中检查唯一WorldDirector（世界编排器）以及区域边界、桃林、桑林、农田、道路、山崖、瀑布水岸、住宅、资源点和人工排除带；复用共享模板，但正式美术MeshSet、PCG实际图及其绑定必须由项目内容包提供。狼、出生、采集和破坏等玩法只允许PCG给候选位置，必须经服务器Gameplay（玩法逻辑）批准。

OpenWorld：已有通用生成源码不能替代实际世界地图。待正式分区/生态方案与地图明确后，再创建和登记真实内容包、Biome（生物群系）/道路/地块/聚落/资源图；不为满足数量创建空插件或假.uasset。

MainArena：现有正式竞技地图不必强制使用PCG，推荐静态烘焙小规模植被、岩石与装饰；1v1至5v5的战斗碰撞、掩体、视野等必须是已审核的确定性静态内容，服务器不运行客户端纯装饰PCG。

登录、UI、英雄技能、玩法规则和后端等不归PCG直接生产；全游戏PCG覆盖只指应程序化生成的世界内容和工具生产链。

## 三、本轮真实实施

1. GamePlatformPCGSpatialDirector.cpp 的BuildStaticExecutionPlan（静态执行计划）新增编辑器原生图绑定检查：Graph（PCG图）加载后必须与UPCGComponent真实Graph、Seed（随机种子）和GenerateOnDemand（按需生成模式）一致。发现旧图或种子漂移即拒绝，不写地图、不调用Generate，不改变专用服务器角色。
2. Build/Validation/VerifyPCGGameCoverage.ps1 新增全世界文件与注册覆盖核查；只读检查65份.uasset及3份.umap，总计68份UE包，输出JSON到Saved/Validation/GamePlatformPCG/PCGWholeGameCoverage.json；默认返回0只表示审计完成，-RequireProductionReady严格模式返回2表示尚无生产验收。
3. Tests/Architecture/TestPCGGameCoverage.ps1 检查三个世界角色、文件集合、证据属性、发布门禁与原生绑定安全规则。不得把PowerShell检查冒充UE5.8真实Automation（自动化）用例。

## 四、剩余生产补完顺序

P0 资产和场景真实性：真实UE Editor独立重开Foundation/Gold图及11份村庄蓝图，验证原生图节点、依赖、唯一Director、地图实例；执行Gold Level G01～G16。

P1 新手村实际生成：正式授权且已审核的树木、岩石、农作物、木构建筑模型接入MeshSet，再把三张外围森林候选图实际连接Spawner与材质、风动、空间排除。正式Gaea高程尚不可用时允许隔离技术审查，禁止使用技术占位资产进入发行包。

P2 新手村关卡：将森林、道路、农田、村庄、水岸、瀑布及禁布置范围按优先级绑定在正式地图，静态Bake并检查导航、碰撞、视线、实例边界、保存冷重启。

P3 开放世界：依据批准地图，建立真实OpenWorld内容包、世界定义、分区及生态、道路、聚落/资源图，复用平台协议。

P4 竞技与网络：主竞技场按需静态PCG。Dedicated Server（专用服务器）仅读取权威静态碰撞、导航、资源/出生候选批准结果；严禁把客户端随机生成用于服务器玩法判定。

P5 发布性能：Client/Server双端独立Cook（资源烘焙），检查WorldPartition（世界分区）、HLOD（分层细节）、HISM（分层实例）、Shader（着色器）、渲染实例数、GPU/CPU耗时、显存、帧率及人工视觉。缺少真实执行证据不得签发3A、60FPS或120FPS达标。

## 五、2026-10-17 新增原生UE5.8核验证据

- 定向UE BuildFoundation编译GamePlatformPCG和GamePlatformPCGEditor两个模块：真正执行了GamePlatformPCGSpatialDirector.cpp编译及编辑器DLL链接，UE UBT结果Succeeded、退出0。构建证据：Saved/Validation/FoundationM0/1155d017-6643-4455-970c-8ee2e925bb6e/Build/Editor/UBT.log。它是PCG模块构建，不是完整Client/Server构建。
- 无头UE编辑器执行GamePlatform.PCG.Editor原生Automation（原生自动化）8项，报告全部Success、失败0。证据：Saved/Validation/GamePlatformPCG/WholeGameReview_20261017/Automation/index.json。
- Gold（独立金标准）首次冷重开因Python脚本提前把UWorld作为普通资产加载，随后切换同一地图导致旧World清理Fatal；修复为仅检查地图路径及反射类，真正地图由EditorLevelLibrary.load_level（编辑器地图切换）加载。又发现Python反射的ValidateParticipantSet（参与者集合校验）在此UE5.8环境下仅返回空OutError（错误文本），原脚本误判为失败；已修正为兼容字符串/元组/布尔结果。
- 第三次独立冷重开返回0，无LogPython异常、成功标记1项；53份非地图资产通过类别与逻辑身份回读，加1张正式Gold地图共54项通过；WorldDirector注册校验通过，地图放置器数量为Director 1、Volume 4、Spline 3、Polygon 2、Connector 2、Exclusion 1。证据：Saved/Validation/GamePlatformPCG/WholeGameReview_20261017/GoldAssetColdReopenVerified.Unreal.log。
- 修正RunGoldLevelAuthoring.ps1（PCG金标准创作入口）：Python阶段不能仅凭UE进程退出0宣布成功，必须从本次Unreal原生日志取得所属阶段的成功标记，并拒绝LogPython: Error及Fatal错误。PowerShell语法及新脚本回归通过。生产G01～G16、Village正式生成、OpenWorld、Cook及真实帧率仍未验收。

## 六、边界及证据

第三方Electric Dreams（官方环境示例）未取得并合法迁移前不得称为已导入；不对锁定UE5.8源码、其他未提交UI资源、已保存地图与已有PCG图执行替换或伪造。静态审计只确认正确UE文件头和内容包注册，不能证明AssetRegistry（资产注册表）、地图中蓝图接线、Spawner、G01～G16、Cook或多人权威已经通过。

当前状态：平台基础较完整；Village部分覆盖；OpenWorld未覆盖；MainArena按需静态PCG；全游戏PCG正式交付尚未完成。
