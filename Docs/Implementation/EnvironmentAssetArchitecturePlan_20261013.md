# 《SSLM》树木、岩石、房屋等环境场景资源配置与插件化实施规划

**版本：** V1.0｜**日期：** 2026-10-13｜**状态：** 规划已编制；实际实施未启动  
**工程：** `DivineBeastsWorkspace`（SSLM工作空间）/`Game/DivineBeastsArena.uproject`（唯一正式游戏工程），UE5.8（虚幻引擎5.8）。

> 本轮仅授权**规划文档**。不导入、制作、下载或替换任何树木/岩石/房屋模型、纹理、材质；不创建或修改C++代码、插件、蓝图、`.uasset`（引擎资产）、`.umap`（关卡）、地图、`.ini`（运行配置）、构建脚本；不编译、不运行PCG（程序化内容生成）、不Cook（资源烘焙）、不发布。以下目录和数据资产是**未来拟建/拟配置**，除非特别写明现有，均不能当成已完成。

## 一、目标与已确认基线

### 1.1 目标

统一管理树木、竹林、灌木、草花、岩石、悬崖、房屋、梁柱门窗、道路、围栏、桥梁、农作物和环境散件。做到：一份美术资源既能人工摆放，也能由现有PCG生成；三层逻辑依赖单向；跨项目可复用机制、不下沉具体SSLM素材；世界间资产归属清晰；后续高精客户端视觉和服务器权威碰撞/导航可以分离；变更可追溯、验证可复核。

### 1.2 现有源码与资源事实（审查快照）

1. `Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json`（内容包登记）现有**18个实际登记项**；其中 `DBAWorldPack_Village`（新手村内容包）已登记。`DBAWorldPack_OpenWorld`（开放世界内容包）与 `DBAWorldPack_MainArena`（竞技场内容包）仅是未来按真实资源交付的归属，不因README目录示意而自动存在。
2. `DBAWorldPack_Village/Content/Maps/L_Village_Start.umap`（正式新手村起始地图）及 `Content/PCG/Blueprints`（PCG放置器蓝图）中森林、建筑等文件存在；**文件存在不等于PCG生成、完整关卡或正式3A美术验收成功**。
3. `GamePlatformPCG`（平台PCG插件）已有 `UGamePlatformPCGMeshSetDefinition`（网格集合）、`UGamePlatformPCGSpawnPolicyDefinition`（生成策略）、`UGamePlatformPCGLayerDefinition`（生成层）、`UGamePlatformPCGBiomePresetDefinition`（群系）、`UGamePlatformPCGProfileDefinition`（生成配置）、`UGamePlatformPCGAssemblyDefinition`（建筑/岩组组合件）及 `UGamePlatformPCGBakeManifest`（烘焙清单）等数据定义合同，**不需新造树木/石头/房屋系统**。
4. `GamePlatformData`（平台数据插件）已经统一管理 `GamePlatformDefinition`（主资产定义）、`FPrimaryAssetId`（主资产身份）、`RequiredDefinitions`（必需依赖）、`PCGGeneration`（PCG生成资产分组）及异步加载租约。
5. `DBAWorlds`（项目世界插件）已有 `UDivineBeastsWorldDefinition`（SSLM世界定义），其 `EnvironmentPCGProfileIds`（环境PCG配置身份）和 `PCGBakeManifestIds`（PCG静态烘焙清单身份）均须满足平台定义与必需依赖约束；纯客户端美术不得直接成为服务器Ready（就绪）的必要依赖。
6. `Game/Config/DefaultGame.ini`（游戏主配置）已扫描 `/DBAWorldPack_Village/Definitions`（世界定义），但**没有明确扫描未来拟建的** `/DBAWorldPack_Village/PCG/Definitions`（PCG数据定义目录）；后续导入时须同步核对AssetManager（引擎资产管理器）扫描与Cook闭包。
7. `Game/Config/Custom/VillageServer/DefaultGame.ini`（新手村服务器打包配置）包含 `+DirectoriesToAlwaysCook=(Path="/DBAWorldPack_Village")`（整包强制Cook），纯视觉资源进入服务器产物的风险未能仅凭配置排除；后续必须用实际依赖审查与客户端/服务器分别烘焙证明。
8. `DBAWorldPack_Village/SourceArt/PCG/TechnicalPrototype_v1`（PCG技术原型源）已有乔木、散石、作物、农篱、农门、木桥技术模型源，非正式3A美术；`SourceArt/PCG/VillagePCGMeshBindings_v1.json`（正式PCG网格映射）本轮查验**不存在**，2026-10-12资源准入记录未通过正式六类资源检查。
9. 平台PCG的既有专项计划 `Docs/Implementation/GamePlatformPCG/PCGExecutionPlan_20261010.md`（PCG实施计划）和 `Docs/Implementation/TerrainGaeaMultiLayerPCGExecutionPlan_20261011.md`（地形与PCG一体化计划）继续有效。本规划只新增“正式环境素材接入及配置”这一项，不复制PCG算法、地形引擎或已有测试执行器。

以上是计划制定时的快照，不是后续执行时的永久事实；任何实际改动前重新核验当前分支、未提交修改、并行任务和资产。

## 二、正式三层归属决策

### 2.1 GamePlatform（平台通用层）

- `GamePlatformData`（平台数据）：主资产身份、版本、发现/异步加载、依赖、租约和取消；禁止新建平行资源管理器。
- `GamePlatformPCG` / `GamePlatformPCGEditor`（PCG共享/编辑器模块）：Schema（生成属性协议）、通用MeshSet、SpawnPolicy、Layer、Biome、Assembly、生成图与校验、烘焙合同，禁止硬编码桃树/十二生肖/新手村路径。
- `GamePlatformSurface`（平台环境表面）：雪、苔藓、湿润、积水和材质参数协调；`GamePlatformWeather`（天气）、`GamePlatformWorld`（世界）、`GamePlatformInteraction`（交互）继续各负其责。
- 不把正式树木、石头、房屋网格或项目材质放到平台插件。

### 2.2 MobaCommon（MOBA通用层）

当前无需PCG中间层，也不为形式创建MOBA树木/建筑Actor（实体）。只有实际存在多MOBA项目复用的竞技场通用空间语义时再评审；Village与OpenWorld不应依赖竞技。

### 2.3 DivineBeasts（项目层）

- `DBAWorlds`（项目世界逻辑）：项目世界定义与配置关系校验，不负责资源下载、第二PCG执行器或大量网格。
- `DBAClient`（项目客户端表现）：在证明真实需求后负责按世界加载/撤销客户端专属视觉资源、适配天气与表面；仍调用平台Data，不新造租约。
- `DBAGameplay` / `DBAServer`（项目玩法/服务器）：玩家通行、出生、采集、可互动门等权威状态和必要碰撞/导航，不能因视觉素材缺失而自动改变游戏规则。
- `DBAWorldPack_Village`（已有新手村内容包）：一期正式树木、岩组、古风建筑、PCG实例与地图的唯一项目内容归属；本轮**不新建**环境内容插件。
- `DBAPresentationPack_Core`（已有公共表现包）：保留已确定VFX和现有材质归属；不得为整理目录而迁移现有材质、改变挂载点。
- 未来 `DBAEnvironmentPack_Common`（公共环境内容包）只在同一正式素材已被至少两个世界复用、重复归属和Cook收益有审计依据时申请；`DBAWorldVisualPack_Village`（新手村客户端视觉包）仅在共享WorldPack无法证明严格服务器资源剥离时申请。**两者只是条件候选，本轮不创建/注册。**

架构原则：**具体资源差异使用DataAsset（数据资产），不是每种树/石/房屋分别建立GamePlatform→MobaCommon→DivineBeasts三层C++类树；没有实际运行行为的静态网格不必新增Actor或Subsystem（子系统）。**

## 三、内容插件目录与命名规划（未来实施，不创建空目录）

唯一物理内容根：`Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content`；在现有资产路径不动的前提下按需增量添加：

```text
DBAWorldPack_Village/                   # 新手村内容插件（已存在）
├── Content/                            # UE内容；虚拟根 /DBAWorldPack_Village
│   ├── Environment/                    # 未来正式环境资源
│   │   ├── Meshes/                      # 三维静态网格
│   │   │   ├── Trees/                  # 桃树、竹林、其他乔木
│   │   │   ├── Vegetation/             # 草、花、灌木
│   │   │   ├── Rocks/                  # 岩石、悬崖、碎石
│   │   │   ├── Buildings/              # 房屋和建筑构件
│   │   │   ├── Structures/             # 道路、桥梁、围栏
│   │   │   └── Props/                  # 环境散件
│   │   ├── Materials/                   # 项目材质及实例
│   │   └── Textures/                    # BaseColor、Normal、ORM等纹理
│   ├── PCG/                            # 程序化资源
│   │   ├── Blueprints/                  # 已有放置器蓝图
│   │   ├── Definitions/                 # 未来生成数据定义实例
│   │   ├── Graphs/                      # 未来生成图实例
│   │   └── Generated/                   # 未来经审核的静态输出
│   ├── Definitions/                    # 已有双端安全世界定义
│   ├── Maps/                           # 已有正式地图
│   └── Development/                    # 技术/审核资源，正式发行排除
└── SourceArt/                          # 原始模型、纹理、许可和来源记录
```

当前已有 `Content/Materials`（地形材质等）及地图、蓝图等保留原路径；上图并不批准搬迁。物理 `Content/Environment/Meshes/Trees`（树木网格）映射引擎虚拟路径 `/DBAWorldPack_Village/Environment/Meshes/Trees`（树木资源挂载目录），不重复填写`Content`。

命名示例（正式资源到位后才使用）：
- `SM_DBA_Tree_Peach_01`（桃树网格）、`SM_DBA_Rock_Cliff_01`（悬崖岩网格）、`SM_DBA_Building_Roof_01`（房屋屋顶网格）。
- `MI_DBA_PeachTree_01`（桃树材质实例）、`T_DBA_PeachLeaf_BaseColor`（叶片基础色纹理）、`T_DBA_PeachLeaf_Normal`（法线纹理）、`T_DBA_PeachLeaf_ORM`（遮蔽/粗糙/金属度纹理）。
- `DA_DBA_PCG_MeshSet_PeachTrees`（桃树网格集合数据资产）、`DA_DBA_PCG_SpawnPolicy_PeachForest`（桃树林生成策略）、`DA_DBA_PCG_Biome_PeachForest`（桃林群系）、`BP_DBA_Building_House_01`（房屋组合蓝图）。
- 文件与非文档目录用英文ASCII；对外字段和接口附中文注释。稳定 `LogicalId`（逻辑身份）与文件名不相等，必须按真实平台Data约束确定，不猜测主资产ID。

## 四、每批正式资源接入与校验合同

### 4.1 来源、质量与审批

美术/技术美术提供原始模型、贴图及来源清单：资源名称/用途、源文件路径、SHA-256、许可证与授权范围、制作或第三方供应方、目标世界、厘米单位/坐标轴、网格尺度与轴心、UV（纹理坐标）、法线/切线、LOD（细节层级）或Nanite（虚拟几何）、PBR（物理材质）贴图、碰撞/导航需求及最终人工美术签审状态。

首批六个既有PCG生产域沿用 `Tools/Unreal/PCG/VerifyVillagePCGProductionSources.py`（新手村正式素材准入器）：
- `Forest.Canopy`（森林乔木）— `ClientCosmetic`（客户端纯装饰）。
- `Rock.Scatter`（岩石散布）— 客户端纯装饰。
- `Agri.Crop`（农田作物）— 客户端纯装饰。
- `Encl.FieldFence`（田篱）— `StaticWorld`（静态权威世界）。
- `Connector.Gate`（农门）— 静态权威世界。
- `Connector.Bridge`（桥梁）— 静态权威世界。

如树干、巨石会阻挡玩家，必须另有静态权威碰撞/导航审批；不能因为叶片或散石是ClientCosmetic就将其当成权威掩体。房屋的领域身份须使用现有PCG已登记的DomainId（领域标识），不自行向底层加项目专属硬编码枚举。

正式映射文件目标为 `SourceArt/PCG/VillagePCGMeshBindings_v1.json`（目前缺失，**后续才创建**）。须遵守既有脚本字段 `domainId`（领域ID）、`meshAssetPath`（真实UE网格路径）、`sha256`（来源校验）、`usage`（用途）；更多授权记录放SourceArt伴随来源清单，需增加强制字段时先升级Schema（数据格式版本）和校验器。`Development`（开发目录）或 `_Tech`（技术网格）不允许伪装为正式资源。

### 4.2 导入引擎与材质

通过真正的UE5.8编辑器导入、保存、独立重新加载，记录尺寸、法线、材质槽、UV、贴图通道/sRGB（色彩空间）、Nanite或LOD实际启用状态和碰撞配置。通用环境雪、苔藓与湿润由 `GamePlatformSurface`（平台表面插件）提供算法，项目专属材质实例只绑定实际参数；不复制独立雪/湿润算法到每一棵树或石头。`.uasset`、`.umap`只准UE工具生成，不能普通脚本或改扩展名伪造。

### 4.3 统一数据资产配置链

正式StaticMesh（静态网格）→ `UGamePlatformPCGMeshSetDefinition`（网格集合）→ `UGamePlatformPCGSpawnPolicyDefinition`（生成策略）→ `UGamePlatformPCGLayerDefinition`（生成层）→ `UGamePlatformPCGBiomePresetDefinition`（群系预设）→ `UGamePlatformPCGProfileDefinition`（生成图配置）→ 经验证的PCG静态输出与 `UGamePlatformPCGBakeManifest`（烘焙记录）。

现有MeshSet条目 `FGamePlatformPCGMeshSetEntry`（网格条目）的 `Mesh`（软网格引用）、`Weight`（选择权重）、`bStaticCollision`（静态碰撞意图）、`bCastShadow`（阴影）、`CullDistanceCm`（剔除距离厘米）、`bNanitePreferred`（Nanite意图）均可复用。布尔字段仅表达配置/意图，真实引擎设置、生成结果、服务器碰撞仍需核对。

已有 `FPrimaryAssetId`（主资产ID）引用必须同步登记 `RequiredDefinitions`（必需依赖），由 `GamePlatformData`（平台数据）统一递归加载，网格与图软引用走 `PCGGeneration`（PCG生成资产分组）。重复身份、环依赖、缺失网格、无效权重、错误端侧、异步取消等必须显式失败或回退，不能随机选一个有效资源。

普通固定手摆的网格可直接使用正式美术资源，不要求每个StaticMesh都创建一套DataAsset。场景需要可配置替换/加权散布/批量装配时才创建对应定义，避免资产数量膨胀。

### 4.4 三个典型场景

**桃树林：** 复用桃树静态网格与材质，使用MeshSet/SpawnPolicy/Layer/Biome配置高度、坡度、密度、道路/耕地排除、比例与种子；叶片和风动归客户端，树干阻挡必须得到静态权威审查。PCG与人工重点树木共用同一正式网格。

**岩石与悬崖：** 大型岩壁优先经人工审核的Nanite或地形静态组合，小石头通过PCG散布；道路、出生区域与桥梁要排除。已有PBR岩石纹理不等同于已具备高品质三维岩石网格；影响掩体/导航的大石必须有服务器一致的静态结果。

**古风房屋：** 门窗、屋顶、柱梁、墙体、地基等网格由项目内容包拥有；先人工组合蓝图并检查尺度、门洞、碰撞、导航，再使用 `UGamePlatformPCGAssemblyDefinition`（现有组合件合同，最多64个稳定插槽）进行已批准构件的相对位置组合或聚落位置选择。PCG插槽不是独立建筑建模器。交互门、采集、仓储通过 `GamePlatformInteraction`（交互）和权威玩法状态实现。

## 五、专用服务器与客户端隔离原则

1. **共享世界保权威，客户端承载视觉**。服务器应保留世界/体验身份、必要碰撞、导航、出生、通行、交互锚点及经审批的静态PCG结果；高精树叶、纹理、材质、纯视觉PCG、VFX只由客户端加载。
2. **先治理现有整包Cook**。未来导入素材前，盘点 `VillageServer/DefaultGame.ini` 中 `DirectoriesToAlwaysCook`（总目录强制烘焙）及 `Build/Game/ServerPresentationExclusions.json`（服务器表现排除规则），评估最小安全包含清单，不能简单移除规则导致服务器地图、雪桥或碰撞丢失。
3. **严审共享地图硬引用**。地图、ExternalActors（外置Actor）、蓝图父类、材质、DataAsset依赖及生成结果若硬引用纯客户端资源，目录NeverCook（禁止烘焙）或软引用标记不能单独证明隔离成功。仅在受控的客户端组合层加载客户端包；若确需拆 `DBAWorldVisualPack_Village`，另行批准并以实际目标Cook验证，当前不创建。
4. **`EnvironmentPCGProfileIds`（世界环境PCG身份）和 `PCGBakeManifestIds`（世界烘焙清单）**只保存满足共享定义合同的必需对象；客户端纯视觉PCG的选择与加载另由客户端上下文处理，避免 `RequiredDefinitions`（递归依赖）间接使专服加载模型。
5. **唯一结论来自真Cook和Stage（暂存）/容器审计**：Editor、Client、Server（三类目标）、AssetRegistry（资产注册）依赖图、IoStore/UnrealPak（打包容器）清单、实际联机与导航检查不能互相替代。

## 六、后续实施执行计划（本轮均为“未启动”）

每个工作包均要求真实源码/资源基线、产物、测试证据和失败回退；工期与成本待正式素材、目标平台和真实测试结果确认，不预设虚假性能阈值。

### P0｜实际资产与引用盘点（未启动）

重新核查最新Git工作树、并行任务、已登记内容包、地图/蓝图/PCG资源及材质；分别输出“正式已验收/技术样板/待导入/待批准/重复/未知”资源台账，盘点服务器整包Cook及引用风险。**验收**：所有相关已知资源有真实路径、所有者和现状证明；不改原资产。

### P1｜目录、来源与配置合同（未启动）

在真实资源到位时确认目标目录、英文命名、源SHA/许可证、版本和替换策略；补齐正式 `VillagePCGMeshBindings_v1.json`（资源绑定），维持单一真实 `ContentPackRegistry.json`（内容包登记）。按实际数据资产存放位置增量修订主资产扫描和Cook配置，明确客户端/服务器目标。**验收**：无非法依赖、路径越界、登记重复或未经批准的空包；旧挂载点保持不变。

### P2｜首批真实模型材质导入（未启动；等待素材）

先接入乔木、散石、作物、围栏、门、桥六类生产资源，再接古风房屋的正式构件。使用现有受控UE编辑器工具真实导入StaticMesh（静态网格）、Texture（纹理）、Material Instance（材质实例）、正确碰撞及实例化设置。**验收**：UE资产保存后独立重开无错误、比例材质正确、来源与版权经批准；不得用技术OBJ（模型源）或Engine BasicShapes（基础模型）替代生产美术。

### P3｜DataAsset与MeshSet配置接线（未启动）

基于既有MeshSet、SpawnPolicy、Layer、Biome、Profile、Assembly建立首批真实定义，配置 `RequiredDefinitions`（必需资产）与 `PCGGeneration`（生成分组），不新建加载器。**验收**：主资产扫描到真实定义、软网格能加载、无依赖环、缺失/取消可恢复且不会影响其他世界租约；Server不载入纯表现。

### P4｜PCG与房屋组合的独立演示（未启动）

复用已有基础模板、11类放置器与Gold关卡规则，先执行独立开发场景的桃林、散石、作物、围栏/门/桥空间生成；房屋先人工完成一栋已审定组合蓝图，再测试组合件定义与聚落定位。**验收**：实际PCG Generate（生成）、保存/重开、道路/农田排除、生成数量与来源指纹可核验；不能以资产文件存在冒充生成成功。

### P5｜正式Village世界静态烘焙（未启动）

将签收的静态结果分区迁入正式Village内容包；对树干、掩体石、围栏、桥、房屋做权威碰撞/导航和出生点审查；对客户端装饰制定不进服务器Ready依赖的装配方案。**验收**：客户端看到正确美术，专服通行/交互/出生与烘焙状态一致；不覆盖现有正式地图和地形雪层链路。

### P6｜编译、双端Cook和性能验证（未启动）

按锁定UE5.8执行Editor/Client/Server实际构建、Data/PCG自动化、全新编辑器资产回读、干净Client/Server Cook及Stage/容器逐项审计；测量Nanite、ISM/HISM（实例化网格）、LOD、Shader（着色器）、GPU/CPU（图形/处理器）、内存、绘制开销和世界流送。**验收**：必要检查有真实退出码/日志，Server无未批准纯视觉资源、权威数据不遗漏；不得把编译、静态脚本、哈希或目录规则写成Cook通过。

### P7｜人工画面与正式交付审核（未启动）

人工检查树木轮廓与比例、岩石表面纹理与体块、古风房屋布局、门窗/桥梁碰撞、PCG重复感、光照、雪苔湿润覆盖、交互与低/中/高画质。修复真实缺陷并签收。**验收**：有原生截图、机器/分辨率、缺陷清单、复测证据和审核结论；未经签收不得宣称正式资源交付。

### 里程碑与依赖

- **M0准备**：P0→P1，梳理已有项目、待导入清单与资源入库合同。
- **M1首批生产资源**：正式素材来源合法并人工批准后执行P2→P3→P4。
- **M2静态世界**：房屋组合和六类场景生成通过后执行P5。
- **M3发行验收**：P6→P7，获得双端容器、性能及视觉真实证据后才批准使用。
- 此流程可复用于以后OpenWorld和MainArena，但不得现在创建未交付空世界包。

## 七、资产替换、失败策略与验收材料

- 替换/重导入前固定Git基线、源SHA-256、原资产备份、唯一所有者及受影响引用/重定向清单；通过UE原生工具迁移，不直接重命名/覆盖二进制。发生Shader、Cook、导航、资源身份或视觉退化时撤销本批变更，保留旧资源可回退。
- **优先风险**：①Village整包Server Cook混入高精美术；②新增PCG定义目录未扫描；③共享世界递归依赖纯客户端模型；④技术源冒充正式美术；⑤房屋门洞/桥梁导航错误；⑥跨世界复用导致重复所有者；⑦并行UE/UBT（引擎构建）覆盖或使用失效DLL。
- 每批交付记录：资源ID/路径/许可与来源、导入设置及网格/纹理实测、数据资产引用关系、Client/Server用途、独立UE重开、PCG实际生成和Bake（静态烘焙）结果、双端Cook和容器清单、性能/人工审核、失败与回退信息。
- 状态仅允许**待资源、待实施、实施中、测试受阻、待人工审核、已签收**；任何测试未执行必须标明未执行。

## 八、本轮规划落地记录

**已执行（仅文档）**：审查现有平台Data、PCG、Surface、项目World、内容包登记和Village打包规则；制定三层职责、资源归属、未来路径和命名、六类首批素材接入合同、桃林/岩石/房屋应用、客户端/服务器边界以及P0—P7分步实施与回退门禁；登记本规划文档入口。

**未执行（留待后续明确授权）**：正式模型/纹理/材质导入、Asset绑定清单创建、PCG数据资产/图/蓝图/地图制作、代码和配置修改、UE编译、Cook、运行与人工验收。

**现存文档问题**：`Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`（总目录规划说明）中有此前遗留的合并冲突标记；本计划只登记独立索引，不解决或改写其他工作流的冲突内容。后续应单独解决并验证。

**下一执行条件**：用户提供或指定后续正式树木、岩石、房屋等资源后，重新核对当时仓库状态，从P0开始逐项实施；不把本次“执行规划”解释为授权立即导入或重构。
