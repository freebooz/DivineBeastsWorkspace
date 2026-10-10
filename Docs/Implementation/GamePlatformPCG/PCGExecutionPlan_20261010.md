# 《神兽联盟》GamePlatformPCG 实施执行计划 V1.0

> 编制日期：2026-10-10。
> 实施状态：**执行计划已形成；本计划描述的 P0～P7 功能工作包尚未据此实施或验收**。
> 唯一工程：DivineBeastsWorkspace（神兽联盟工作空间）/Game/DivineBeastsArena.uproject（神兽联盟正式工程）。
> 引擎基线：UE5.8（虚幻引擎5.8），实际版本、脚本和构建目标以工程锁定配置为准。
> 核心插件：Game/Plugins/GamePlatform/World/GamePlatformPCG（游戏平台程序化生成插件）。
> 本文件用于直接分派实现任务、记录阶段门禁；原有插件 Docs/改造方案与执行计划.md（设计规格）保持正式架构基线。
> 注意：下文“工单P0～P7”表示实施工作包；“原语P0～P9”表示已定义的PCG原语，二者不能混淆。

## 一、任务总目标

在现有 GamePlatformPCG（游戏平台PCG）基础上增量打通“配置定义 → 官方PCG图 → 原生采样/生成 → 空间冲突处理 → 网格实例化 → 编辑器烘焙 → 保存/重开 → 资产与地图校验 → Client/Server（客户端/服务器）干净Cook（资源烘焙） → 性能审核”的完整生产链。平台算法可跨游戏复用；《神兽联盟》项目层只保留具体内容、布局、数据定义实例及项目校验。

优先级固定：**M0：P0+P1 → M1：P2+P3 → M2：P4 → M3：P5+P6 → M4：P7**。P0～P3为第一批落地与实际生产门禁，只有真实Gold Level（金标准关卡）验收通过，才进入World Partition（世界分区）和高级空间生成。

本次用户授权仅为制定计划：**不编辑PCG C++业务代码、不运行模板生成、不创建.uasset（资源文件）或.umap（地图文件）、不更改Cook配置、不提交或推送**。后续获得实施授权后方按本计划逐包修改。

## 二、执行前提与真实基线

### 2.1 已有内容——保留复用

1. GamePlatformPCG.uplugin（PCG插件描述）声明 GamePlatformPCG（共享运行模块）与 GamePlatformPCGEditor（编辑器模块），依赖官方PCG、GamePlatformCore（平台核心）、GamePlatformData（平台数据）、GamePlatformWorld（平台世界）。
2. 已实现 Schema v1（元数据属性协议）、原语P0～P9、16项WorldStage（世界阶段）枚举、Domain ID（领域标识）、PriorityTable（优先级表）、Override（人工覆盖）及通用Definition（数据定义）。
3. 已有 UGamePlatformPCGProfileDefinition（生成配置定义）、UGamePlatformPCGBakeManifest（烘焙清单）、IGamePlatformPCGService（生成服务接口）、UGamePlatformPCGWorldSubsystem（世界作用域子系统）、图/结果审查、作用域句柄及取消清理。
4. 已有 VolumeActor（体积放置器）、SplineActor（样条放置器）、PolygonActor（地块放置器）、ConnectorActor（连接件放置器）、ExclusionActor（排除放置器）、WorldDirector（世界编排器）及首批环境节点。
5. 已登记13个模板ID，其中M0/M1生成器实现12个基础模板；已编写7个公共子图生成器；未开放的TPL_RailingAttached（附着栏杆模板）留到M2。
6. Legacy（旧版）固定四节点生成与审查路径继续保留，不能未经兼容性测试直接删除。
7. Build/Validation/VerifyPCGArchitecture.ps1（架构静态检查）、VerifyPCGGoldLevelPrerequisites.ps1（金标准前置检查）在2026-10-10审查中执行并返回0；只表明专项静态检查通过，不代表UE真实编译、资产验收或Cook完成。

### 2.2 明确缺口——不能误判为已交付

1. 1.0模板当前的生成图主要是Schema处理和初步节点组合；MeshSetId（网格集合ID）到已租约Definition、实际Mesh Selector（网格选择器）及StaticMeshSpawner（静态网格生成器）的链路尚未成为已验证的正式生产能力。Private/Validation/PCGGraphInspection.cpp（PCG图审查）仍对新版模板的Runtime（运行时）执行返回Unsupported（不支持）。
2. 尚无已确认实际落盘并独立重开的Foundation模板.uasset、PCG_GoldLevel_M1.umap（金标准地图）和按G01～G16完成的场景证据。
3. 实际项目层 DBAWorlds（神兽联盟项目世界插件）尚未发现正式PCG接线代码；正式内容包登记中已存在DBAWorldPack_Village（新手村内容包），OpenWorld（常驻世界）及MainArena（竞技世界）内容包的真实资产与登记不能视作已完成。
4. 道路缓冲区/闭合地块、几何交叉、桥/门断口、上游排除掩码以及Director的完整阶段协调尚需开发/实证。
5. 旧 Build/Validation/VerifyPCG.ps1（综合验证脚本）仍检查当前PCG目录不存在的一批旧Docs文件，需要按现行资料修订，且须保留“静态通过不等于生产通过”的状态语义。
6. UE Editor/Client/Server三目标编译、UE Automation（自动化测试）、实际DataValidation（数据校验）、Client/Server独立Cook、多世界运行和性能预算均需要当前版本证据。
7. 现有PCG世界校验器主要迭代已加载Actor；World Partition未加载Actor描述符验收要在M2增补。

### 2.3 禁止事项

- 不新建平行的ProjectPCG、DivineBeastsPCG或空的Core/Nodes/Network/Biome/World模块；继续两模块内按 Public（公开接口）/Private（私有实现）划分。
- 不将桃林、生肖、村落专属美术和项目地图资源放入GamePlatform（平台层）。
- 不让GamePlatformPCG管理第二份资产身份、世界实例目录、NavMesh（导航网格）、雨雪/材质/VFX（视觉特效）或服务器玩法状态。
- 不将P0～P9原语扩成P10等新编号；不重排16阶段与Schema已发布字段；不制造.uasset/.umap文本占位。
- 不将相同Seed（随机种子）当成跨端同一权威地形、碰撞、出生或采集状态的证明；Dedicated Server（专用服务器）不得执行纯装饰PCG。
- 不删除其他开发者未提交变更、地图外置Actor/对象数据，不在未经许可时自动提交、推送、部署或重启服务器。

## 三、三层逻辑架构、端侧与资源归属

### 3.1 第一层 GamePlatform（游戏平台通用层）

- GamePlatformPCG（共享运行）：Schema/Primitive/Domain（协议/原语/领域）、Definition、通用节点、PriorityCarve（优先级挖洞）、公开服务/Actor（实体）合同、请求句柄、每世界生命周期及安全失败。
- GamePlatformPCGEditor（仅编辑器）：真实UPCGGraph（官方PCG图）创建、官方节点装配、Template Contract（模板合同）、AssetRegistry（资产注册）、DataValidation（数据校验）、Bake Manifest（烘焙清单）、源与输出指纹、金标准关卡工具。
- GamePlatformData（数据插件）作为唯一主资产身份、RequiredDefinitions（必需定义）依赖闭包和Asset Bundle（资源分组）租约服务；PCG使用PCGGeneration（生成资产分组）并严格区分端侧。
- GamePlatformWorld（世界插件）提供WorldId/RegionId（世界/区域身份）、ContextGeneration（上下文代次）、区域查询/流送事实，PCG不得自存全局世界清单。
- GamePlatformNavigation（导航插件）只消费获批静态障碍和服务器导航事实；GamePlatformSurface（表面材质插件）、GamePlatformWeather（天气插件）及GamePlatformVFX（视觉特效插件）各保留自身机制。PCG只决定生成地点/内容选择与必要环境数据。

### 3.2 第二层 MobaCommon（MOBA通用层）

本期无PCG新代码和新插件。竞技专属项目内容复用平台采样/道路/掩体语义。只有出现多个MOBA项目共享的稳定竞技空间类型时，才按项目规则单独评审可选中间层扩展；Village、OpenWorld不应因此依赖竞技插件。

### 3.3 第三层 DivineBeasts（神兽联盟项目层）

- DBAWorlds（项目世界机制插件）负责项目世界定义、组合、场景校验、已审查PCG输出的可选适配；不重写采样或官方PCG执行器。
- DBAWorldPack_Village（新手村世界内容包）拥有桃林、农田、田篱、道路、桥、码头、湖心三岛环境及玩法候选的Graph Instance（图实例）、Data Asset（数据资产）、Child Blueprint（子蓝图）和正式地图。
- DBAWorldPack_OpenWorld（常驻世界内容包）、DBAWorldPack_MainArena（主竞技场内容包）仅在确有真实资产/消费者、完成ContentPackRegistry.json（内容包注册表）登记与Client/Server Cook审查后创建；不预建空包。
- 纯配置差异使用平台已有Definition实例。无需结构变化不新增项目C++子类；平台只向下被依赖，绝不反向引用项目资源。

### 3.4 生成及权威链

    World/Region（世界与区域上下文）
       ↓
    FieldProvider（只读地形/群系输入）
       ↓
    WorldDirector（阶段及参与者合同）
       ↓
    Definition/Graph Instance（数据定义/图实例）
       ↓
    GamePlatformData递归租约 → 官方PCG节点
       ↓
    Priority/Exclusion（优先级/空间排除）
       ↓
    Mesh Selector/Spawner（网格选择与生成）
       ↓
    Output Audit（输出审查）/ Bake Manifest（烘焙清单）
       ↓
    Save/Reopen（保存/重开）/ DataValidation（数据校验）
       ↓
    Client/Server干净Cook → 世界静态交付

- Editor Static（编辑器静态生成）为一期默认：涉及碰撞、导航、出生、采集和胜负的结果需一致烘焙/权威验证。
- Client Cosmetic（客户端纯装饰生成）仅用于无碰撞、无导航、无玩法权限副作用的本地内容，必须支持取消、世界释放、租约归还。
- Dedicated Server（专用服务器）不执行纯装饰生成，不Cook纯展示用的PCG材质、贴图及特效。
- P9 State（状态原语）只提供稳定初始锚点/标识；真实门、砍伐、收割和资源库存由既有服务器Gameplay/Interaction/Save（玩法/交互/存档）权威管理。

## 四、工单及里程碑

### M0 / P0：工程基线与门禁收敛（阻断级）

**前置条件：** 读取AGENTS.md及任务路径生效规则，检查主分支、HEAD、工作树和并行任务；保留所有无关未提交修改。

**任务：**

- P0-01：核对GamePlatformPCG.uplugin、GamePlatformPCG.Build.cs、GamePlatformPCGEditor.Build.cs（插件和模块构建描述）、正式.uproject、Editor/Client/Server Target（构建目标）的真实依赖、模块名、载入阶段及端侧限制；审查反向依赖/公共头泄漏。
- P0-02：梳理Schema v1、DomainCatalog（42领域）、PrimitiveModel（10原语）、现有Definitions/Nodes（定义/节点）与文档的真实差异，形成带源码文件、行号、影响、可重现条件的整改清单；已有功能不重复创建。
- P0-03：修订Build/Validation/VerifyPCG.ps1（PCG综合验证脚本）中过时的文档列表，确保验证的是现行设计/交付资产；不能伪造旧文件以取悦检查。
- P0-04：完善VerifyPCGArchitecture.ps1（架构检查）、VerifyPCGGoldLevelPrerequisites.ps1（金标准前置检查）和相关Tests/Architecture（三层架构测试）：无平行PCG、目标端侧、Schema/Template合同、Definition依赖、禁止模板运行旁路、真实资产验收状态独立标记。
- P0-05：审查源码和脚本的中文注释、公开类型与字段/异常/异步/权威语义，并同步对应插件Docs；不因注释缺失大规模重写无关插件。
- P0-06：串行协调现有UE构建作业后执行可用的静态门禁、原生C++测试和UE5.8 Editor/Client/Server编译。每条命令记录版本、退出码、失败日志；全局构建互斥或环境缺失标记“阻断”，不得假装源码编译失败或成功。

**修改边界：** PCG插件Docs/README、Build/Validation中PCG相关脚本、PCG专项测试与为修复实际错误必需的局部源码；其余插件原则上只读。

**交付：** 基线审计报告、真实差异单、修复的验证入口、分目标编译结果、P1放行检查单。

**退出条件：** 专项静态门禁和文档门禁通过，三层/端侧无新增违例，已记录实际可运行编译或清晰阻断；**不等于真实模板资产验收**。

**回退：** 仅还原本工单受控文件的增量差异，不能git reset/clean整个工作树。

### M0 / P1：Foundation模板、真实Definition、Mesh生成与烘焙最小闭环（阻断级）

**前置：** P0门禁完成，确认锁定UE5.8可加载PCGEditor，不存在并行UE构建/资产保存冲突。

**任务：**

- P1-01：核查FGamePlatformPCGSchema（属性注册）、FGamePlatformPCGAttr（字段访问器）和PCGMetadata（PCG元数据）的实际类型、缺字段失败、Schema Major（主版本）兼容，P9 MutableId（状态稳定ID）尚未实证则保持保留。
- P1-02：审查并显式运行当前GamePlatformPCGFoundationTemplatesCommandlet（模板生成命令），对占用路径先整体预检；由UE Editor/Commandlet真实生成并保存位于 /Game/Development/Foundation/PCG/Templates（开发模板）中的12个M0/M1模板，以及 /Game/Development/Foundation/PCG/Subgraphs（公共子图）中的7个子图。M2 TPL_RailingAttached（附着栏杆）不提前创建。
- P1-03：重开并审查图节点、输入Pin（引脚）、唯一SchemaWriter→SchemaValidator→Output（属性写入→校验→输出）可达路径、bIsTemplate（模板标志）、CPU（中央处理器）模式和禁止未知节点合同。
- P1-04：打通 MeshSetId（网格目录ID）→ UGamePlatformPCGMeshSetDefinition（网格集合定义）→ RequiredDefinitions（统一必需定义）/GamePlatformData租约 → 合法Mesh Selector（网格选择器）→ StaticMeshSpawner（静态网格生成器）。不能停留在“Metadata属性中写了MeshSetId”就宣称Mesh已生成。
- P1-05：以平台ExecPreset（执行预设）、MeshSet（网格集合）、SpawnPolicy（生成策略）、Layer（生成层）、BiomePreset（群系预设）、ExclusionPreset（排除预设）、PriorityTable（优先级表）创建真实最小资产，证明只换Definition实例即可换树种、岩石和密度。
- P1-06：在GamePlatformPCGGraphInspection（图审查）内保留0.1.0 Legacy（旧四节点）兼容路径，完善批准图的编辑器静态生产/输出检查。新版Runtime（运行）不满足生产完整合同时继续明确Unsupported（不支持），不得走旧图CastChecked（强制转换）触发断言。
- P1-07：补充source fingerprint（源指纹）的RequiredDefinitions递归闭包、资源内容版本、图版本、插件版本、源/输出可审计摘要及Bake Manifest；引入输出Owner（所有者）撤销规则，禁止清理其他生成批次或手摆Actor。
- P1-08：补UE Automation（引擎自动化）：正常、缺定义、未知Schema、网格缺失、禁用节点、超量点、零密度、请求取消、世界关闭、重复请求、租约释放和“旧版夹具仍可运行”回归。

**主要代码范围：** GamePlatformPCG的Public/Schema、Public/Definitions、Private/Nodes、Private/Validation/PCGGraphInspection.cpp、Private/Subsystems/GamePlatformPCGWorldSubsystem.cpp；Editor模块Private/Authoring、Manifests、Validators、Tests。

**交付：** 可保存/独立重开的真实图与Definition，最小森林/岩石静态实例、正确Spawner、资产指纹/烘焙清单与自动化证据。开发模板仍位于Development，正式模板归属与CanContainContent（插件是否可含内容）另行审查，禁止无证直接改插件描述。

**退出条件：** 完整“Definition→PCG Graph→Mesh实例→Bake→保存→独立重开→审计”通过，并证明兼容旧版路径。未完成任何关键环节均不得放行P2生产推广。

**回退：** 以版本化模板/配置切换保留Legacy路径；只清理自己明确拥有的测试输出。

### M1 / P2：Field、优先级总线、空间几何与世界阶段编排

**前置：** P1真实模板、数据租约和静态生成最小闭环通过。

**任务：**

- P2-01：以原语P0 Field（场）对接Landscape（地形）高度/坡度、Biomes（群系权重）、Buildable（可建造/可耕）掩码等只读输入；M1不写地形。
- P2-02：建立Spline（样条）道路缓冲区、Polygon（多边形）地块、门/桥占位和ManualLock（人工锁定）、PaintExclude（人工刷除）的空间掩码源；跨节点使用Schema统一属性而不是散落字符串。
- P2-03：保留UGamePlatformPCGPriorityTableDefinition（优先级定义）为唯一真源，按照严格较高优先级覆盖较低优先级；ManualLock100、GameplayExclusion90、Connector80、MajorRoad70覆盖Canopy30、Crop20、GroundCover5。同优先级不互切且必须确定性拒绝冲突或显式人工决策。
- P2-04：完善P2 Linear（线性）与P4 Parcel（地块）的样条采样、多边形裁剪、路肩、田垄、栏柱、端点/拐角、跨度、门洞打断和几何交叉标签输入；现有FitPostsToSpline（样条布柱）、BreakSpansByTags（标签打断）、BuildRows（田垄）继续复用。
- P2-05：WorldDirector只承担显式参与者、固定阶段、依赖输入、错误中断、来源归属和结果检查，不再造一套官方PCG任务调度器；校验唯一Director、SourceId（来源ID）/Schema版本、跨World（世界）注册。
- P2-06：写覆盖测试：道路穿森林、道路穿农田、圈地围栏、门打断、手摆桥占位、坡度边界、零面积地块、反向样条、重复SourceId、同级冲突、漏注册/跨世界参与者。

**主要范围：** Private/Nodes（规则/节点）、Public/Services/GamePlatformPCGPriorityRules.h（优先级规则）、Public/Actors/GamePlatformPCGActors.h（放置器合同）、Private/Actors（实际编排）、GamePlatformPCGEditor/Private/Validators（编辑器检查）。

**交付：** 可复用空间掩码、互斥/优先级子图、阶段依赖合同及可复现测试。

**退出条件：** 典型路切林、路切田、围栏插门等冲突符合规定；未知Geometry（几何数据）或不兼容Schema安全失败，不遗留部分结果。

**回退：** 模板版本/配置独立启用新空间规则；不覆盖正式地图已有手摆资产。

### M1 / P3：Gold Level金标准关卡与Village项目落地

**前置：** P2空间规则具备实际可执行节点/模板与测试。

**任务：**

- P3-01：在 /Game/Development/Foundation/PCG/Validation（开发验证目录）通过UE Editor创建真实PCG_GoldLevel_M1.umap（M1金标准地图），不放进正式Shipping（发行版）内容包。
- P3-02：按现有Docs/GoldLevelAcceptance.md（金标准关卡验收规范）放置唯一WorldDirector、森林/资源两个Volume（体积）、至少一处Exclusion（排除区）、至少三条Spline（道路/小径/田篱）、一个闭合Polygon（农田）、至少两个Connector（农门/手摆桥或断口）；所有放置器显式注册到唯一Director。
- P3-03：创建真实Definition实例：ExecPreset1、PriorityTable1、MeshSet不少于3、SpawnPolicy不少于2、Layer不少于3、BiomePreset1、ExclusionPreset1、RoadProfile1、EnclosureProfile1、ParcelPreset1、CropProfile1、ConnectorCatalog1。所有有效主资产依赖均在RequiredDefinitions声明。
- P3-04：使用经P1验证的TPL_ScatterSurface（散布）、TPL_BiomeGenerator（群系）、TPL_LinearDresser（线性装饰）、TPL_Enclosure（围合）、TPL_Connector（连接件）、TPL_GateInsert（插门）、TPL_ParcelFill（地块）、TPL_CropField（农田）、TPL_InterfaceBand（界面带）与7个公共子图进行真实生成。
- P3-05：逐项执行G01～G16，不得跳项：G01道路切林、G02小径切林、G03道路切田、G04围栏连续、G05门洞、G06手摆桥、G07人工锁定、G08排除区、G09同Seed、G10网格替换、G11策略替换、G12非法Schema、G13缺依赖、G14多个Director、G15未注册放置器、G16未开放Runtime模板须安全Unsupported（如果合同升级须同步评审该项）。
- P3-06：按第三层归属，在已登记DBAWorldPack_Village（新手村世界内容包）创建桃林、农田、路径、门、围栏、资源点的Data Asset（数据资产）、Graph Instance（图实例）及Child Blueprint（子蓝图），由DBAWorlds（项目世界插件）仅负责项目世界组合；保留当前正式入口和世界准入链路。
- P3-07：资产保存后关闭独立Editor重开，核对地图外置Actor、资产父类、内容包注册、Bake Manifest、碰撞、NavMesh（导航网格）、出生位置和资源锚点与实际服务器事实一致。
- P3-08：执行Editor/Client/Server真实构建、Client/Server干净Cook及产物审查；双客户端+一个Dedicated Server（专用服务器）加载同一受控场景，禁止服务器执行纯装饰PCG或携带纯表现材质，核验取消/退出/多实例清理。

**交付：** 金标准地图、G01～G16结果、项目新手村增量、保存重开/导航/双Cook/双客户端证据。

**退出条件：** 16项功能验证真实通过，三目标编译、资产重开、Cook/服务器权威和性能记录均无阻断。未达标不得标记M1生产完成。

**回退：** 首次引入Village采用可撤销的配置/图实例/关卡增量，不重建现有村庄地图、世界身份与外置Actor。

### M2 / P4：湖泊河流、开放世界分区与层级生成

**前置：** P3多系统场景与双端Cook具备真实生产验收。

**任务：**

- P4-01：以P0/P2/P4/P6原语配置Water.River（河流）、Water.Lake（湖泊）、Water.Bank（河湖岸带），并扩展Forest.Edge（林缘）、Agri.Orchard（果园）、Rock.Formation（岩组）、Encl.RoadGuard（道路栏杆）、Encl.CliffRail（临崖栏杆）、Encl.DockRail（码头栏杆）。
- P4-02：PCG只控制岸边岩石、植被、码头周边和环境装饰的布点/选择；水体几何与河湖系统归当前已采用的世界水体机制；雪/苔藓/湿润归GamePlatformSurface，雨雪状态归GamePlatformWeather，瀑布粒子归GamePlatformVFX。
- P4-03：引入World Partition（世界分区）和Data Layer（数据层）后，核查未加载Actor描述符与整个分区资产集；现有TActorIterator（已加载Actor迭代）不足以单独证明全图唯一Director、全部登记和稳定SourceId。
- P4-04：按实测需要评估HiGen（分层生成）、HLOD（层级细节代理）、RuntimeDetail（客户端运行时细节）、流入流出、重复生成、分区边界、取消与缓存。未通过仍使用非分区静态Bake。
- P4-05：以“湖心三岛桃花新手村”为项目组合目标：主要湖心岛/悬崖/古建筑人工审定或离线制作，PCG用于局部树木、草地、岸石、田园和过渡环境，不允许客户端随机改变岛屿地形权威。
- P4-06：补分区烘焙、地图重开、Data Layer（数据层）过滤、NavMesh及客户端/服务器一致性、内存峰值与高密度场景性能报告。

**交付：** M2生态与界面带模板、真实分区验证地图、分区审计和性能记录；需要正式OpenWorld包时先确认真实资产、登记和Cook策略，禁止空建。

**退出条件：** 分区加载/卸载无丢失、重复或跨世界污染，导航和静态碰撞一致，客户端纯装饰被正确隔离。

**回退：** 保留M1非分区静态生成路线；生成失败或流送问题时恢复已验证Bake Manifest版本。

### M3 / P5：古风聚落、建筑组合与玩法候选锚点

**前置：** P4通过或局部原型明确限制在P3已验收的静态地图。

**任务：**

- P5-01：使用P4 Parcel（地块）+P5 Assembly（组合件）实现Settle.Parcel（宅基地）、Settle.Building（建筑）、Settle.Yard（院落）及Encl.Paddock（圈舍围栏）、Encl.YardWall（院墙）、Encl.HedgeRow（绿篱）。
- P5-02：古风房屋采用人工审核的模块/组合件配置，平台不包含神兽联盟特定柱式、装饰、纹理或建筑摆位；道路通达、庭院边界、水岸退界由显式规则检验。
- P5-03：实现Play.Resource（资源）、Play.Cover（掩体）、Play.Climb（攀爬）、Play.Spawn（出生）的候选位置生成/稳定标识及与地图几何的合法性审查。
- P5-04：玩法相关结果交Gameplay（玩法）、Interaction（交互）、Navigation（导航）和Dedicated Server（专用服务器）审批后生效；PCG不得决定资源发放、出生准入、采集资格或竞技胜负。
- P5-05：按世界内容包划分古风聚落/资源/玩法图实例；MainArena竞技地图保留可审核的固定布局，兼容1v1至5v5，不把线上比赛变成动态权威地形PCG。
- P5-06：审查艺术一致性、房屋间距、道路连通、碰撞/导航、AI通行和多端场景一致性。

**交付：** 聚落与玩法候选模板、项目内容资产、服务器合法性校验合同和场景实测。

**退出条件：** 空间布局符合限制，不创建项目专属底层算法或无消费者的空类型，玩法最终权威由服务端提供。

**回退：** 按地图/内容包版本撤销对应项目资产，不修改平台已发布原语和Schema身份。

### M3 / P6：P9稳定ID、交互状态与存档

**前置：** P5具备实际可互动对象，GamePlatformSave（平台存档）和GamePlatformInteraction（平台交互）已有合法服务端消费路径。

**任务：**

- P6-01：固定职责：PCG负责静态候选与稳定标识；服务器Gameplay/Interaction（玩法/交互）决定资源采集、砍伐、门的真实状态；GamePlatformSave负责持久化，不建PCG独立状态数据库。
- P6-02：稳定对象身份绑定World/Region/Source/Template版本及候选语义，禁止用数组位置、UObject指针或随机顺序作为跨次加载存档Key（键）。
- P6-03：验证UE5.8的PCG Metadata（元数据）对FGuid（全局唯一标识）的支持、写入、保存、重开；不支持时采用明确安全的替代结构，不能声称已实现GUID写入。
- P6-04：服务器处理State.Harvest（收割）、State.Chop（砍伐）、State.Gate（门）、State.Persist（持久状态），包含幂等、防重试重复发奖、异常取消、进程重启与分区流入流出恢复。
- P6-05：客户端仅消费服务器已确认的状态快照/复制更新；本地可选表现失败不影响采集结算或权威门状态。
- P6-06：双客户端、断线重连、资源重新加载、服务器重启、旧回调、重复消息、内容版本变更与迁移用例全部覆盖。

**交付：** 可变对象稳定身份规范、服务端状态映射、恢复与迁移规则、跨世界/存档回归证据。

**退出条件：** 服务端权威一致，已采集对象不因PCG再生成而重复复活/发放，旧版存档有明确迁移/拒绝机制。

**回退：** 保留上一版布局与状态定义；不兼容更新要求先审批迁移和备份，不能静默清除历史。

### M4 / P7：自动桥、路堑、洞穴与空间图

**前置：** M0～M3整体生产链稳定，并且存在明确的高级生成消费者。

**任务：**

- P7-01：增强P3 Connector（连接件）原语的道路/河流求交候选，人工或自动校验桥跨、连接头和道路衔接；稳定性不足时保留ManualConnector（手摆连接件）优先。
- P7-02：P7 Cavity（体腔）覆盖地基、洞穴和Cut/Fill（挖填）评估；TerrainWrite（地形写入）是高风险独立改造，须先留可恢复的正式地形/图层备份及版本对照。
- P7-03：P8 SpatialGraph（空间图）实现可验证的室内、地牢、走廊/出入口拓扑合同及导航合法性；内容资源归项目世界包。
- P7-04：GPU（图形处理器）节点仅在锁定UE5.8实际支持、性能瓶颈明确、数据与执行模型可审查时试点；CPU（中央处理器）回退及版本可重现必须提供实证。
- P7-05：完成自动桥非法跨度、道路断连、洞穴自交、地形破坏、室内不可达、分区裁剪、服务器导航差异和长稳压力测试。

**交付：** 高级空间插件内功能、项目场景样例、地形回退、性能与网络安全审查；不将M4结果提前算作1.0完成。

**退出条件：** 拓扑、碰撞、导航、性能、分区及资产回退均有真实证据。

**回退：** 单功能灰度开启；失败保留人工地标及原先已批准的静态Bake（烘焙）结果。

## 五、原语P0～P9、42项Domain（领域）与工单映射

此处是已有GamePlatformPCG/Docs/DomainCatalog.md（领域目录）和PrimitiveModel.md（原语目录）的完整归集，**不表示要建立42个新C++类**。

1. **Field（地形与场，3项，原语P0）**：Field.Height（高程场）→P1/P2；Field.BiomeWeight（群系权重）→P1/P2；Field.Buildable（可建/可耕掩码）→P2。
2. **Forest（森林植被，4项，原语P1/P6）**：Forest.Canopy（乔木）、Forest.Understory（林下灌木）、Forest.Floor（草本地被）→P1/P3；Forest.Edge（林缘）→P4。
3. **Rock（岩石，2项，原语P1/P5）**：Rock.Scatter（散石）→P1/P3；Rock.Formation（岩组）→P4/P5。
4. **Agri（农业，4项，原语P4/P1）**：Agri.Parcel（农地）、Agri.Crop（农作物）→P2/P3；Agri.Orchard（果园）、Agri.Fallow（休耕地）→P4。
5. **Road/Path（道路路径，4项，原语P2/P6）**：Road.Network（道路网络）、Road.Surface（道路表面）、Path.Trail（林间小径）→P2/P3；Road.Shoulder（路肩）→P2/P3。
6. **Water（水域，3项，原语P2/P4/P6）**：Water.River（河流）、Water.Lake（湖泊）、Water.Bank（河湖岸带）→P4。
7. **Bridge/Gate/Break（连接件，3项，原语P3/P2）**：Bridge.Span（桥跨）→P3手摆/P7自动；Gate.Farm（农门）、Break.Road（道路打断围合）→P2/P3。
8. **Enclosure（围栏边界，7项，原语P2/P6）**：Encl.FieldFence（田篱）→P2/P3；Encl.Paddock（圈舍栏）、Encl.YardWall（院墙）、Encl.HedgeRow（绿篱）→P5；Encl.RoadGuard（路护栏）、Encl.CliffRail（崖栏杆）、Encl.DockRail（码头栏杆）→P4。
9. **Settlement（聚落，3项，原语P4/P5）**：Settle.Parcel（宅基地）、Settle.Building（建筑）、Settle.Yard（院落）→P5。
10. **Gameplay（玩法，5项，原语P1/排除）**：Play.Resource（资源锚点）→P3/P5；Play.Cover（掩体）、Play.Climb（攀爬）、Play.Spawn（出生）→P5；Gameplay.Exclusion（硬排除）→P2/P3。
11. **State（状态，4项，原语P9）**：State.Harvest（收割）、State.Chop（砍伐）、State.Gate（门状态）、State.Persist（持久化）→P6。

其中原语P7 Cavity（体腔）和P8 SpatialGraph（空间图）由实施工单P7处理；不能为了增加领域数量提前虚构已完成的新ID。定义对象优先以既有DataAsset（数据资产）和Graph Instance（图实例）表达，增加节点前必须验证是否能由官方PCG和现有参数组合完成。

## 六、执行管线、优先级与安全边界

### 6.1 已固定16阶段

    FieldRead（读取场）
      → TerrainWrite（写地形，M0/M1禁止）
      → WaterBody（水体）
      → Networks（道路网络）
      → CutFillRequest（挖填请求，M0/M1禁止）
      → Connectors（连接件）
      → Parcels（地块）
      → Enclosures（围合）
      → Buildings（建筑）
      → Interiors（室内，M0/M1禁止）
      → Scatter（散布）
      → GameplayAnchors（玩法锚点）
      → InterfaceBands（过渡界面带）
      → ApplyState（应用权威状态，M0/M1不执行）
      → StreamHooks（流送钩子）
      → RuntimeDetail（仅客户端纯装饰）

阶段编号不得在1.x内重新排序；阶段存在不代表该阶段已经具备生成生产能力。Director（编排器）只组织生成依赖与结果有效性，不取代官方PCG执行调度。

### 6.2 优先级

UGamePlatformPCGPriorityTableDefinition（PCG优先级定义）为配置真源，现有默认规则应保留：ManualLock100（人工锁定）、GameplayExclusion90（玩法硬排除）、MajorGate82（主要门）、Connector80（连接件）、MajorRoad70（主路）、RoadGuard68（路护栏）、WaterBody60（水体）、YardWall55（院墙）、Parcel50（地块）、FieldFence48（田篱）、MinorRoad40（小路）、Canopy30（乔木）、Crop20（作物）、InterfaceBand10（过渡带）、GroundCover5（草地）。

PriorityCarve（优先级挖洞）只允许严格更高层剪切更低层，同级不互切。同键/同优先级冲突须明确失败或人工Override（覆盖），不依赖无序遍历。Freeze（冻结）、PaintExclude（刷除）、ForceSpline（强制样条）、ManualConnector（手摆连接件）是编辑器行为，不是新原语。

### 6.3 性能与生命周期

- 优先复用官方PCG执行器和ISM/HISM（实例化静态网格/层级实例化静态网格），不得重造第二套调度、资源池、AssetManager或世界全局目录。
- 每图记录点数、实例数、CPU/GPU耗时、PCGComponent（组件）数、批次、生成/清理耗时、Cook包大小、Draw Call（绘制调用数）、峰值内存和世界分区流入流出尖峰。
- 现有FrameBudgetMs=4、MaxPointsPerExecution=65536、MaxComponentsPerBatch=64及Legacy MaximumOutputs=4096属于代码/配置初值，不代表目标PC/移动端已批准预算；由真实代表性场景测量后确定配额，不能编造阈值。
- 检查现有WorldSubsystem约0.05秒的Ticker（定时检查）：无活动请求时若考虑暂停轮询，应先记录基准数据，并确保加载回调、取消、超时、清理及终态通知无漏派发。
- 跨地图/世界/PIE（编辑器内运行）按作用域句柄+代次清理；清理失败不能宣布Cleaned（已清理），不能提前归还仍被原生组件依赖的租约。

## 七、验收门禁（每个工作包必须逐项标记）

**A. 架构和源码：** 校验AGENTS、三层继承方向、插件/模块依赖、公开/私有头、资产归属、中文注释、Schema/Template版本。执行Build/Validation/VerifyPCGArchitecture.ps1（架构门禁）等已存在入口并记录返回码。静态扫描不是资产/引擎验收。

**B. UE编译与测试：** 使用锁定UE5.8，分别执行DivineBeastsArenaEditor、DivineBeastsArenaClient、DivineBeastsArenaServer的Win64 Development（Windows 64位开发构建）；运行本地C++策略测试及UE Automation。编译、UHT（头文件工具）、链接和引擎自动化应分开记录。现有VerifyPCG.ps1即使分项通过也可能以退出码2标记总体验收未完成，报告必须保留此语义。

**C. 真实PCG资产：** .uasset/.umap仅由UE编辑器/Commandlet生成，AssetRegistry（资产注册）可发现，Definition正确加载，节点和Graph Pin（图引脚）可复核，结果可保存、关闭独立Editor重开，Bake Manifest、碰撞、NavMesh、外置Actor齐全。

**D. Gold Level（金标准场景）：** 必须逐项实测G01～G16，分别保留场景截图、生成实例统计、指纹、验证命令和失败日志；G16仍应证明未获批准的Runtime模板无法越过安全边界。

**E. Dedicated Server（专用服务器）与Cook：** Client/Server干净Cook，对实际包清单审查纯材质/装饰资产不进入Server，权威出生/碰撞/采集/导航使用同一受控Bake或服务器真实权威；在OpenWorld/Village/MainArena三服务器角色上按真实场景验证。MainArena兼容1v1～5v5共用同一构建目标，不另造比赛专服或独立PCG模块。

**F. 稳定性和性能：** 同版本同Seed重生成；取消/超时/销毁/网络重连/双客户端/多个世界/分区流入流出；CPU/GPU、内存、资产和烘焙开销有实际测量；人工检查村庄文化、美术比例、道路畅通和视线可读性。

**单包完成定义：** 上游门禁已通过；只变更本工作包批准文件；功能与异常路径有可复现测试；有真实编译/资产/场景/Cook/性能分项结果；中文注释/文档同步；剩余缺陷列出风险与责任；无P0阻断项。未执行的门禁不可标记“通过”。

## 八、必须覆盖的测试场景与证据

- S01：同一森林Scatter（散布）模板更换MeshSet/SpawnPolicy后不复制图，实例网格/密度按Definition改变。
- S02：道路与森林交叉，树木不侵入道路；ManualLock（人工冻结）不能被自动生成覆盖。
- S03：农田被道路穿越，作物不侵入道路，围栏及田埂连续。
- S04：围栏按跨度放置、按门标签打断，端柱/转角与长度规则正确。
- S05：人工桥、道路、湖岸交叉不互相侵占，碰撞和导航一致。
- S06：非法Schema、缺失RequiredDefinitions、重复标识/Director或漏注册Actor失败关闭。
- S07：取消、超时、世界销毁和已完成结果释放，无其他世界或人工资产被清理。
- S08：相同引擎、插件、图、Definition、世界/区域/源身份、Seed和版本时，点数/变换/OutputFingerprint（输出指纹）一致；不承诺跨UE版本二进制一致。
- S09：双客户端+Dedicated Server加载同一静态生成结果，服务器不执行纯装饰PCG，碰撞/导航/出生合法。
- S10：World Partition未加载Actor审计、分区卸载与重入、HLOD/运行时资源和取消清理一致。
- S11：客户端断线重连及服务器重启后砍伐/采集/门状态从权威存档恢复，不出现重复资源发放。
- S12：高密度/低密度、PC不同性能档/移动端目标的生成与内存测量；首次上线阈值由性能数据和审批准则确定。

每条证据至少包含：工单ID、版本/HEAD与工作树状态、实际引擎版本、图与主资产身份及内容修订、运行地图、World/Region/Seed、命令与退出码、耗时、资产指纹/截图/日志、通过或阻断状态、人工复核结论。不得用文字声明代替未执行的真实UE测试。

## 九、风险、回退与执行纪律

1. **并行工作：** 2026-10-10工作树存在其他任务未提交修改，执行前重新检查，严禁覆盖天气、战斗反馈、登录、新手村等并行成果。只编辑精确路径和读取快照确定的内容。
2. **UE构建并发：** 遇UBT（虚幻构建工具）全局Mutex（互斥锁）或编辑器占用须记录外部阻断，不自行关闭其他任务进程，不伪报源码错误；验证依真实失败输出判断。
3. **UE5.8 API风险：** 必须根据实际安装版本检查UPCGGraph、PCGSettings、PCGMetadata（图/节点配置/元数据）接口，禁止从其他版本示例无验证迁入。
4. **内容包风险：** 正式资产按ContentPackRegistry.json登记；路径、父类、蓝图硬引用与软路径需要同时进行资产级验证。
5. **兼容风险：** 0.1.0 Legacy Profile（旧生成配置）与旧图保留已验证的回退；1.0新模板采用受控版本、合同及迁移策略，不直接改已发布反射路径或属性编号。
6. **权威风险：** PCG提供初始候选空间，Server负责真实碰撞、导航、采集、出生、门/资源状态；绝不让Runtime Cosmetic（运行时纯装饰）影响玩法。
7. **地形风险：** M4写地形/挖填前必须备份原地形/图层；只有明确所有权的输出才能撤销，不使用全仓git reset/clean，也不误删外置Actor。
8. **性能风险：** HiGen/GPU/RuntimeDetail在实测、目标支持和资源闭包未验证前关闭，稳定非分区静态Bake作为保底。
9. **版本回退：** 每个工单输出影响清单、启用开关/版本、失败恢复和旧资产兼容策略；未过门禁只能保持上一批准状态，不能直接上线。
10. **中文审阅：** 所有新增或修改的一方源码、接口、参数、错误码、异步所有权、测试、脚本及文档写清中文职责、条件、单位、边界和“为什么”。

## 十、交付材料、进度更新与后续实施入口

每个工单提交一个独立、可人工复核的执行记录，必须包含：
- 执行前基线：HEAD、分支、git status/diff（版本状态/差异）、有效规则、原始问题、调用方和数据所有权。
- 变更范围：实际修改源文件/数据/地图、三层归属、端侧、是否引入依赖、为何不复用已有结构。
- 验收：脚本与命令/退出码、UE编译、Automation、真实资产保存/重开、Cook、网络权威、性能及人工检查分别列状态（通过/失败/未执行/受阻/不适用）。
- 收尾：修复缺陷、未解决问题、回退/迁移策略、下一包准入判断。禁止没有证据即改为已完成。

参考文档与位置：
- AGENTS.md（工作空间工程规范）；Game/Plugins/插件开发规范.md（插件实施规范）。
- Docs/Architecture/解决方案总体规划.md（架构上位规划）。
- Game/Plugins/GamePlatform/World/GamePlatformPCG/Docs/改造方案与执行计划.md（平台PCG原始详细设计）。
- 同插件Docs/DomainCatalog.md（42领域）、PrimitiveModel.md（P0～P9原语）、SchemaV1.md（属性协议）、Nodes.md（节点）、Templates.md（模板）、GoldLevelAcceptance.md（G01～G16验收）。
- Docs/Implementation/GamePlatformPCG/ExecutionProgress.md（历史实施进度，不可改写旧记录）。
- 本文件 Docs/Implementation/GamePlatformPCG/PCGExecutionPlan_20261010.md（正式P0～P7执行工单）。

**首次实施指令：** 在获得“按计划执行”授权后先执行P0。重新读取最新工程状态及生效规则，对现存文件进行精确检查，生成局部修改清单，增量实现并运行当次允许的真实验证；P0放行后P1→P2→P3，P3过Gold Level及双端Cook后才启动M2。每轮更新实施进度，不自动提交、推送或部署。

> **本轮交付只完成计划编制、文档归集；未执行P0～P7功能实现。**
