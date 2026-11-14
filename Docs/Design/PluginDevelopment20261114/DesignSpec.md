# SSLM 插件开发关卡与资源包设计规格

状态：推荐方案，待人工确认。日期：2026-11-14。审查依据见 `AuditReport.md`；命名遵循 `NamingStandard.md`。

## 方案选择

按用户最新要求，采用 **一个统一插件 HUB → 70 个插件专属页面 / 薄根关卡＋现有统一执行器＋共用测试设施**。全部插件从同一个 HUB 选择进入；根关卡仅装配该插件所需夹具与共享环境。模型、材质、生命周期和服务执行逻辑不按地图复制。服务类采用轻量中性诊断场景；编辑器工具显示宿主限制并使用受控编辑器适配。统一入口的视觉和交互见 [PluginHubDesign.md](PluginHubDesign.md)。

统一 HUB 采用三列大卡片与纵向滚动，上图下说明、不分页，不显示目录、搜索或侧栏详情，各插件保留独立调试环境。现有 Hub 原位改版为唯一可见入口；现有 70 个 Widget 通过原位改版/必要原生改名复用，不复制整套玩家、UI、世界管理或服务逻辑。

另规划 8 个资源审核场景：树木、灌木草类、花卉地被、岩石、整栋建筑、模块构件、道具、材质色彩。78 项逐关卡 UI 的建议路径、页面及具体功能类别均见 `LevelCatalog.json` 和 `Review.html`。这是审批用派生索引；实施时写回既有 PluginDemoManifest，不成为第二份运行注册表。

## 三层逻辑与所有权

| 层 / 所有者 | 负责 | 禁止 |
|---|---|---|
| GamePlatformUI / Data / DeveloperTools 等平台插件 | 通用界面主题与生命周期、资产加载租约、验证能力、各领域公开接口 | 引用 DBA 页、植物模型或具体项目规则 |
| MobaCommon / GamePlatformArena / MobaPresentation | 通用竞技规则和竞技表现语义 | 依赖项目界面包；把非竞技插件强制装配成 MOBA |
| DBAClient / DivineBeastsDemoClient | 既有目录、ViewModel、适配器、开发运行与项目装配 | 复制平台执行器、资产管理器、VFX 播放器或建立第二套调试框架 |
| DBAUIPack_Core | 项目开发 UI 样式和真实 Widget 资产 | 持有服务端权威或绕过业务资格 |
| 项目内容包 | 模型、纹理、实例、定义及其来源记录 | 反向依赖项目代码私有实现；以空目录代表资源已完成 |
| Game/Content/Development/PluginDemos | 薄关卡装配和开发夹具 | 成为正式新手村/竞技场的依赖或被默认发行 Cook 带入 |

依赖只沿 `DivineBeasts → MobaCommon → GamePlatform` 或项目直接到平台；同层也无环。根关卡是第三层装配入口，不把游戏装配代码搬进主模块。继续消费 `IDivineBeastsDemoScenarioAdapter.Prepare/Execute/Stop`、`UDivineBeastsDemoCatalogSubsystem`、`UDivineBeastsDemoHubScreen` 和现有事件 ViewModel。

## 项目目录与资源包

以下为建议目录。现有带下划线的挂载身份保持兼容；新建文件和新插件名不带下划线。新包只有首批真实资产、来源记录和目标隔离齐备后才注册，不预建空包。

```text
Game/Plugins/DivineBeasts/ContentPacks/Presentation/  # 项目表现内容分类
├── DBAFoliagePack_Core/                             # 已登记植物包，保留身份
│   ├── Content/Foliage/                            # 现有植物内容
│   │   ├── Trees/                                  # 树木
│   │   ├── Shrubs/                                 # 灌木
│   │   ├── Grass/                                  # 草与蕨类
│   │   ├── Flowers/                                # 花卉与地被分类
│   │   ├── Materials/                              # 植物母材质与实例
│   │   ├── FoliageTypes/                           # 原生植被配置
│   │   └── PCG/                                    # 中立于具体地图的植物集合
│   ├── Content/Vendor/                             # 已有第三方资产与原身份
│   └── SourceArt/                                  # 来源、授权及导入原稿
├── DBAEnvironmentPackCore/                          # 拟建公共自然环境包
│   ├── DBAEnvironmentPackCore.uplugin               # 内容插件描述，无空 C++ 模块
│   ├── Content/                                    # 引擎资产
│   │   ├── Rocks/                                  # 巨石、卵石、岩壁、碎石
│   │   ├── Ground/                                 # 可复用地表模型与材质集
│   │   ├── Props/                                  # 通用环境道具外观
│   │   ├── Materials/                              # 本包材质及实例
│   │   ├── Textures/                               # 本包纹理
│   │   └── Definitions/                            # 数据服务加载的定义
│   ├── SourceArt/                                  # 模型原文件与纹理源图
│   └── Docs/                                       # 来源、规格与生成记录
└── DBAArchitecturePackCore/                         # 拟建公共建筑和构件包
    ├── DBAArchitecturePackCore.uplugin              # 真实叶子插件
    ├── Content/                                    # 引擎资产
    │   ├── Buildings/                              # 可复用整栋建筑
    │   ├── Components/                             # 模块构件
    │   │   ├── Walls/                              # 墙体与真实洞口
    │   │   ├── Doors/                              # 门与独立门扇
    │   │   ├── Windows/                            # 窗框、玻璃及开洞
    │   │   ├── Roofs/                              # 屋面与檐口
    │   │   ├── Columns/                            # 柱、梁及连接件
    │   │   ├── Stairs/                             # 台阶与坡道
    │   │   ├── Bridges/                            # 桥梁构件
    │   │   └── Fences/                             # 围栏、围墙与门洞
    │   ├── Assemblies/                             # 可编辑的装配蓝图
    │   ├── Materials/                              # 建筑材质和实例
    │   ├── Textures/                               # 建筑贴图
    │   └── Definitions/                            # 建筑及构件定义
    ├── SourceArt/                                  # 建模和贴图原稿
    └── Docs/                                       # 尺寸、来源与原生记录
```

世界专属摆放、湖泊、地形、道路、导航、出生点、碰撞规则和 WorldDirector/PCG 实例仍归 DBAWorldPack；公共包不引用世界包。UI、VFX、音频和角色保留已有各自包，不并入一个巨大资源包。公共 Surface 函数依然归 GamePlatformSurface，项目纹理/风格材质按各内容包归属，Surface/VFX/UI 材质域分开。

先整理现有 41 个植物网格，通过分类引用展示已有供应商花草，保持网格唯一副本。首批新增优先补灌木、正式岩石及一套可真实拼装的建筑构件；具体采购或下载不属于本次授权。世界专属草屋是否提升为公共模型，应由实际复用场景和原生引用检查决定，不能单凭文件名迁移。

每条资源登记 Owner、原生类型、软引用、来源/授权、导入设置、单位、轴心、Bounds、材质槽、LOD/Nanite、碰撞、纹理通道、风/动画合同、引用世界和验收状态。厘米为单位，UE Z 轴向上；构件先明确吸附网格与净开口尺寸，不能照概念图推定实际尺寸。模型实例化优先复用同一网格；性能由真实场景测量，未测数字显示未知。

## 统一开发 UI

- 顶栏：插件、场景和运行宿主；场景/参数/检查/证据四页。左侧当前插件的功能与案例，不再通过长菜单寻找其他插件。
- 中部：可折叠的 3D 观察区或服务状态图；固定相机、重置视点、基准/当前切换。服务流程图呈现事件结果，不伪造三维业务对象。
- 右侧：当前定义提供的参数类型、单位、有效范围、可写资格及来源。示例值不能成为新的运行默认真源；非法值给具名错误。编辑前快照和当前值分开。
- 底部：准备、执行、停止、恢复、采集证据。仅显示适配器确实支持的操作，不能为所有服务造假 Pause/Resume。恢复仅撤销本次拥有的临时状态，不回滚已提交交易或服务器事实。
- 状态明确区分源码、资源加载、依赖就绪、已执行、失败、停止、原生保存重载、Cook、人工确认；不使用一个绿色“通过”覆盖全部阶段。
- 初次绑定取一次快照，后续使用目录、参数结果、视口/输入和生命周期通知；失活/换世界解绑。禁止 Widget Tick/定时轮询业务数据。
- 视觉沿用玄墨青、碧玉、鎏金与已批准透明按钮。正常、悬停、按下、禁用各态复用统一样式；没有新主题执行器。原生页面保持已有固定字号与控件尺寸，设计图的显示缩放不改变合同。

效果母版见 `Concepts`，78 张具体页面见 `Pages` 与 `Previews`。母版中的装饰模型、资产数量、轴向文字和参数仅为美术布局示意；最终精确数值、Z 向上坐标、文件命名和调用资格以本规格及已有真实合同为准。

## 性能与发布边界

只加载当前插件/案例，退出释放本次租约；多关卡共用网格/材质/函数，切换时不预加载全部内容包。内容列表沿用虚拟化；角色/模型预览视口在隐藏时停用相应表现，不用常驻刷新业务状态。记录加载峰值、切换释放、Draw Call、实例数、材质/透明过绘、CPU/GPU 时间；预算需基线实测后批准。

审核关卡和界面只进入显式 PluginDemoClient 配置；正式前端与 Dedicated Server 的剥离需实际 Cook/Stage 容器证明。必要碰撞和权威定义遵循服务端资源合同，不能因为模型归表现包就全部裁掉。若启动本地联机人工验证，沿用 StartLocalTestClients、真实 Village/MainArena 版本门禁和十个可用快速登录按钮；凭据不写入文件、日志或命令行，`t.MaxFPS=0`、`r.VSync=0` 实际回读。

确认本规格和界面后按 ImplementationPlan 执行。本轮没有新增真实关卡、原生 Widget 或内容插件，没有修改正式场景。
