# 47个代码插件及17个内容包演示覆盖清单（P0发现阶段）

数据来自真实.uplugin及现行架构文档。**本轮尚未验证所有功能、真实地图运行或人工验收。**

字段“文档能力线索”不代表运行通过；每个插件的具体FeatureId（功能标识）待进一步从真实公开API、蓝图资产、实际操作独立分解。


## 平台层代码插件（40）

| 插件 | 推荐演示模板 | 文档能力线索（未实测） | 当前状态 |
| --- | --- | --- | --- |
| GamePlatformApplicationFlow | ServiceDiagnostics | GameInstance（游戏实例）作用域流程执行器；Definition、节点、分支、异步完成、有限重试、超时、取消、代次和跨地图生命周期。 | 待功能核验、待演示及人工验收 |
| GamePlatformInput | UIWidgetGallery | LocalPlayer作用域 Enhanced Input 服务；可扩展 SemanticId/Descriptor、平台 Built-in 公共语义、ProfileCompiler→CompactSlot、Context/Block… | 待功能核验、待演示及人工验收 |
| GamePlatformLoading | UIWidgetGallery | 冻结 DAG Loading Task（加载任务）图、Required/Optional/Degradable策略、Data Lease（数据租约）、WorldPresence屏障、按需低开销Ticker、运行诊断、订阅与可扩展任务工厂。 | 待功能核验、待演示及人工验收 |
| GamePlatformLocalization | UIWidgetGallery | 当前仅模块入口，尚无完整本地化运行体系。 | 待功能核验、待演示及人工验收 |
| GamePlatformSave | ServiceDiagnostics | GameInstance 级非权威本地存档服务；稳定逻辑 Key、Namespace Provider、双层版本、Schema 迁移、SHA-1 文件身份、CRC32 损坏检测、线程池异步 IO、同槽并发门禁、临时文件 Full Fl… | 待功能核验、待演示及人工验收 |
| GamePlatformSettings | UIWidgetGallery | Runtime 已实现类型安全 Descriptor／Provider／Registry／分层解析／Snapshot／ChangeSet／Migration／异步持久化编排；Client 已实现 `UGameUserSettings`… | 待功能核验、待演示及人工验收 |
| GamePlatformDebug | ServiceDiagnostics | Development/Test（开发／测试）诊断、Debug Command（调试命令）、Gameplay Debugger（玩法调试器）、Provider Registry（提供者注册表）和客户端调试面板；Shipping（正式发… | 待功能核验、待演示及人工验收 |
| GamePlatformDeveloperTools | EditorAuthoring | Data Validation（数据校验）、资产审计、命名／依赖／继承边界门禁、Commandlet（命令行工具）、CI（持续集成）规则、性能基线和统一 Review Harness（审核宿主）。 | 待功能核验、待演示及人工验收 |
| GamePlatformTelemetry | ServiceDiagnostics | Event/Metric（事件／指标）、Schema Registry（模式注册）、Sampling（采样）、Privacy Filter（隐私过滤）、Bounded Buffer（有界缓冲）、Network Sink（网络输出）、T… | 待功能核验、待演示及人工验收 |
| GamePlatformCore | ServiceDiagnostics | 平台中立 ID（标识）、结构化 ErrorCode（错误码）、Result（结果）、Version（版本）、VersionRange（兼容区间）、基础算法和日志契约；保持 Core/CoreUObject 最小依赖。 | 待功能核验、待演示及人工验收 |
| GamePlatformData | ServiceDiagnostics | `UGamePlatformDefinitionBase（平台定义基类）`、稳定 Primary Asset（主资产）身份、统一 Asset Manager（资产管理器）、异步依赖加载、Demand Ledger（需求账本）、作用域 … | 待功能核验、待演示及人工验收 |
| GamePlatformLobby | GameplaySandbox | 当前只有三个模块入口；不代表独立 Lobby（大厅）服务器。 | 待功能核验、待演示及人工验收 |
| GamePlatformVillage | GameplaySandbox | 当前只有三个模块入口。 | 待功能核验、待演示及人工验收 |
| GamePlatformAI | GameplaySandbox | AI Definition、状态／目标组件、感知、稳定目标选择、BehaviorTree/Blackboard（行为树／黑板）、Patrol/Chase/Attack/ReturnHome（巡逻／追击／攻击／返回）和服务器权威执行。 | 待功能核验、待演示及人工验收 |
| GamePlatformAbilitySystem | GameplaySandbox | 基于原生 GAS（Gameplay Ability System，游戏能力系统）的 ASC、Ability Set（能力集）、Grant Ledger（授权账本）、标签、输入桥接、Activation Gate（激活门禁）和 Avat… | 待功能核验、待演示及人工验收 |
| GamePlatformAnimation | WorldVisualStage | 当前运行代码仍为模块壳；已形成 3A 动画实施规格。 | 待功能核验、待演示及人工验收 |
| GamePlatformCharacter | GameplaySandbox | Hero Definition（英雄定义）、Definition Loader（定义加载）、Character Creation Provider（角色创建提供者）、模块化角色初始化器契约和跨层只读角色状态接口；空 `GamePlat… | 待功能核验、待演示及人工验收 |
| GamePlatformCombat | GameplaySandbox | Combat AttributeSet（战斗属性集）、Combat Component、Damage/Healing Execution（伤害／治疗执行）、GameplayEffect、护盾、控制、死亡、命中验证和 Combat Ev… | 待功能核验、待演示及人工验收 |
| GamePlatformGameplay | GameplaySandbox | Experience（体验）、Eligibility（资格）、Pawn Definition、GameMode/GameState/PlayerController 基类、可信加入、出生、玩家生命周期和复制快照。 | 待功能核验、待演示及人工验收 |
| GamePlatformQuest | GameplaySandbox | Quest/Objective Definition（任务／目标定义）、状态机、OwnerOnly Snapshot（仅拥有者快照）、客户端追踪、服务器事件索引、幂等、完成与持久化边界。 | 待功能核验、待演示及人工验收 |
| GamePlatformOnline | ServiceDiagnostics | 在线公开契约、认证 Provider（提供者）生命周期、配置、认证状态机、Profile（资料）、请求／结果和诊断基础。 | 待功能核验、待演示及人工验收 |
| GamePlatformServer | ServiceDiagnostics | Dedicated Server（专用服务器）Register（注册）、Heartbeat（心跳）、Ready（就绪）、Drain（排空）、HTTP 控制 Provider 和 GameInstance 生命周期子系统。 | 待功能核验、待演示及人工验收 |
| GamePlatformSession | ServiceDiagnostics | Private（私有）连接状态内核，包含 Join/Transfer/Reconnect（加入／迁移／重连）意图、代次、Deadline（截止）、Assignment（分配）和取消／失败规则。 | 待功能核验、待演示及人工验收 |
| GamePlatformCommerceUI | UIWidgetGallery | 商业客户端子系统、Gateway HTTP Transport（网关HTTP传输）、ViewModel（视图模型）、页面和商业 UI 类型。 | 待功能核验、待演示及人工验收 |
| GamePlatformEntitlement | GameplaySandbox | Entitlement Definition（权益定义）、查询、客户端子系统、Gateway 传输和客户端缓存类型。 | 待功能核验、待演示及人工验收 |
| GamePlatformEquipment | GameplaySandbox | 装备组件／定义、视觉定义／组件、服务器装备组件、GAS Grant Port（能力授予端口）和端侧拆分。 | 待功能核验、待演示及人工验收 |
| GamePlatformInventory | GameplaySandbox | 背包客户端子系统、Item Definition（物品定义）、Gateway 传输、筛选／投影所需类型；不拥有权威库存。 | 待功能核验、待演示及人工验收 |
| GamePlatformLiveOps | ServiceDiagnostics | LiveOps 客户端子系统、Server Time Estimator（服务器时间估算）和 Gateway 传输基础。 | 待功能核验、待演示及人工验收 |
| GamePlatformProgression | GameplaySandbox | Progression Track Definition（成长轨道定义）、Snapshot Provider（快照提供者）、客户端子系统和 Gateway 传输。 | 待功能核验、待演示及人工验收 |
| GamePlatformCamera | WorldVisualStage | 当前运行代码为模块壳；已形成 LocalPlayer 作用域 3A 相机实施规格。 | 待功能核验、待演示及人工验收 |
| GamePlatformPresentation | WorldVisualStage | 中立 Presentation Request（表现请求）、Context（上下文）、Catalog（目录）、Provider 注册和客户端表现子系统。 | 待功能核验、待演示及人工验收 |
| GamePlatformSFX | WorldVisualStage | 当前运行代码为模块壳；已形成基于 UE Audio/MetaSounds（虚幻音频／MetaSounds）的完整实施规格。 | 待功能核验、待演示及人工验收 |
| GamePlatformSurface | WorldVisualStage | 环境表面状态、每世界MPC桥接、Blueprint/C++入口、八个稳定全局参数、真实MPC生成/校验Commandlet，以及雪／苔藓／湿润／积水／坡度／高度／世界噪声的通用材质资产合同。 | 待功能核验、待演示及人工验收 |
| GamePlatformUI | UIWidgetGallery | Screen Definition（页面定义）、Route（路由）、Layer Stack（层栈）、Dialog（对话框）、Loading Screen（加载界面）、ViewModel 基类和 UI Manager Subsystem。 | 待功能核验、待演示及人工验收 |
| GamePlatformVFX | WorldVisualStage | 统一 VFX 运行框架；10种行为 Definition、17类内容分类、Catalog/Resolver、GamePlatformData Lease、Niagara Executor（Niagara执行器）、实例句柄、池化、Sca… | 待功能核验、待演示及人工验收 |
| GamePlatformInteraction | GameplaySandbox | Interactable/Interactor（可交互对象／交互者）组件、Focus Rule（焦点规则）、Eligibility Provider（资格提供者）和 Interaction Event（交互事件）。 | 待功能核验、待演示及人工验收 |
| GamePlatformNavigation | GameplaySandbox | Agent Profile（代理配置）、导航服务、Nav Area（导航区域）、Modifier（修改器）、Query Filter（查询过滤器）、Smart Link（智能链接）和世界子系统。 | 待功能核验、待演示及人工验收 |
| GamePlatformPCG | EditorAuthoring | 当前已实装 PCG Profile（配置）、Bake Manifest（烘焙清单）、Graph Inspection（图检查）、Policy（策略）、World Subsystem（世界子系统）和 Editor Library（编辑器… | 待功能核验、待演示及人工验收 |
| GamePlatformWeather | WorldVisualStage | 数据化天气预设、单世界服务器权威调度、量化过渡快照网络复制；客户端Surface桥接和VFX/SFX中立表现请求。 | 待功能核验、待演示及人工验收 |
| GamePlatformWorld | ServiceDiagnostics | World/Region Definition（世界／区域定义）、World Context、区域注册、Streaming（流送）、Readiness（就绪）和 World Subsystem；不承担全局服务器分配。 | 待功能核验、待演示及人工验收 |

## MOBA层代码插件（2）

| 插件 | 推荐演示模板 | 文档能力线索（未实测） | 当前状态 |
| --- | --- | --- | --- |
| GamePlatformArena | GameplaySandbox | MOBA通用竞技唯一插件；统一 1v1～5v5 MainArena（主竞技场）模式、竞技 Definition、Roster（名单）、阶段状态、比分/KDA、断线重连、弃权、结果构建和 Client/Server 适配。 | 待功能核验、待演示及人工验收 |
| MobaPresentation | WorldVisualStage | 将 Arena/Combat/Ability/Status/Character（竞技／战斗／技能／状态／角色）已确认事实转换成平台中立表现请求；不直接播放 Niagara、Sound（音效）或创建 Widget（界面）。 | 待功能核验、待演示及人工验收 |

## SSLM项目代码插件（5）

| 插件 | 推荐演示模板 | 文档能力线索（未实测） | 当前状态 |
| --- | --- | --- | --- |
| DBAArena | GameplaySandbox | 项目竞技 Definition、项目 Eligibility（资格）、客户端竞技流程扩展和 MainArena 项目服务器适配；默认可选，关闭后公共项目闭包不依赖竞技。 | 待功能核验、待演示及人工验收 |
| DBAClient | UIWidgetGallery | 项目应用流程装配、`DivineBeasts.Input.*` 输入语义/Profile/事件桥、项目表现上下文／映射、UI 路由和 ViewModel；公共客户端不强制依赖竞技。 | 待功能核验、待演示及人工验收 |
| DBAGameplay | GameplaySandbox | 项目稳定身份、Shared（共享契约）目录、十二生肖 Hero Definition、Hero Catalog（英雄目录）、角色组件、创建 Provider 和出生初始化；不强制依赖竞技。 | 待功能核验、待演示及人工验收 |
| DBAServer | ServiceDiagnostics | 读取并校验三 ServerRole Profile（服务器角色配置）、Experience、地图与必需资源；调用 GamePlatformServer 完成 Register/Heartbeat/Ready/Drain。 | 待功能核验、待演示及人工验收 |
| DBAWorlds | WorldVisualStage | `UDivineBeastsWorldDefinition（SSLM世界定义）`，约束 ServerRole（服务器角色）、Experience（体验）和可选 ArenaMode（竞技模式）一致性。 | 待功能核验、待演示及人工验收 |

## 项目内容包（17）

| 插件 | 推荐演示模板 | 文档能力线索（未实测） | 当前状态 |
| --- | --- | --- | --- |
| DBAContentPack_Common | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Boar | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Dog | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Dragon | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Goat | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Horse | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Monkey | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Ox | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Rabbit | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Rat | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Rooster | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Snake | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAHeroPack_Tiger | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAFrontEndPack | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAPresentationPack_Core | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAUIPack_Core | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |
| DBAWorldPack_Village | AssetGallery | 文档未覆盖，待功能级审计 | 待功能核验、待演示及人工验收 |

## 详细功能审核后续门禁

1. 阅读对应插件公开接口、运行实现和蓝图，将一项插件功能拆成独立FeatureId；登记真实接口、资产、参数和失败/重置操作。
2. 逐张打开既有31个开发地图验证AssetRegistry、运行效果与可操作性；诊断探针不得标为完整演示。
3. 与各插件当前成熟度交叉核对；只有真实实现的功能才进入可操作演示；未实现模块如实标识。
4. 对接自动化证据与人工签署记录，缺失结果不得标通过。
